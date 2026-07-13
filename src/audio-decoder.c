/*
 * AirPlay audio decoder using FFmpeg (libavcodec)
 *
 * Decodes AAC-ELD / AAC-LC / ALAC from AirPlay into float32 PCM
 * that can be fed to OBS via obs_source_output_audio().
 */

#include "audio-decoder.h"

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

#include <util/base.h>

#include <stdlib.h>
#include <string.h>

/* AudioSpecificConfig blobs (MPEG-4 ISO 14496-3 1.6.2.1), matching
 * UxPlay's canonical caps for the AirPlay audio streams. */
static const uint8_t ASC_AAC_ELD_44100_2[] = {0xF8, 0xE8, 0x50, 0x00}; /* spf 480 */
static const uint8_t ASC_AAC_LC_44100_2[] = {0x12, 0x10};              /* spf 1024 */

/* ALAC magic cookie: 44100/16/2, spf 352 */
static const uint8_t ALAC_COOKIE_44100_16_2[] = {
	0x00, 0x00, 0x00, 0x24, 'a',  'l',  'a',  'c',
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x60,
	0x00, 0x10, 0x28, 0x0a, 0x0e, 0x02, 0x00, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xac, 0x44,
};

struct audio_decoder {
	AVCodecContext *ctx;
	AVFrame *frame;
	AVPacket *pkt;
	struct SwrContext *swr;

	/* swr input config cache - rebuilt when the decoded frame changes */
	int swr_in_rate;
	int swr_in_channels;
	int swr_in_fmt;

	/* Output buffer */
	float *pcm_buf;
	size_t pcm_buf_size;

	unsigned char ct; /* current AirPlay compression type, 0 = not set */
	int err_count;    /* decode errors logged so far (rate limit) */
};

static void log_av_error(const char *what, int err)
{
	char buf[64] = {0};
	av_strerror(err, buf, sizeof(buf));
	blog(LOG_ERROR, "[AirPlay] %s: %s (%d)", what, buf, err);
}

struct audio_decoder *audio_decoder_create(void)
{
	struct audio_decoder *dec = calloc(1, sizeof(struct audio_decoder));
	if (!dec)
		return NULL;

	dec->frame = av_frame_alloc();
	dec->pkt = av_packet_alloc();
	if (!dec->frame || !dec->pkt) {
		audio_decoder_destroy(dec);
		return NULL;
	}

	/* Initial PCM buffer (1024 samples * 2 ch * 4 bytes) */
	dec->pcm_buf_size = 1024 * 2 * sizeof(float);
	dec->pcm_buf = malloc(dec->pcm_buf_size);
	if (!dec->pcm_buf) {
		audio_decoder_destroy(dec);
		return NULL;
	}

	return dec;
}

void audio_decoder_destroy(struct audio_decoder *dec)
{
	if (!dec)
		return;

	if (dec->swr)
		swr_free(&dec->swr);

	free(dec->pcm_buf);

	if (dec->frame)
		av_frame_free(&dec->frame);
	if (dec->pkt)
		av_packet_free(&dec->pkt);
	if (dec->ctx)
		avcodec_free_context(&dec->ctx);

	free(dec);
}

static bool open_codec(struct audio_decoder *dec, unsigned char ct)
{
	const AVCodec *codec = NULL;
	const uint8_t *extradata = NULL;
	size_t extradata_size = 0;

	switch (ct) {
	case 2: /* ALAC */
		codec = avcodec_find_decoder(AV_CODEC_ID_ALAC);
		extradata = ALAC_COOKIE_44100_16_2;
		extradata_size = sizeof(ALAC_COOKIE_44100_16_2);
		break;
	case 4: /* AAC-LC */
	case 8: /* AAC-ELD */
		/* libfdk_aac if present (best ELD support), else native */
		codec = avcodec_find_decoder_by_name("libfdk_aac");
		if (!codec)
			codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
		if (ct == 8) {
			extradata = ASC_AAC_ELD_44100_2;
			extradata_size = sizeof(ASC_AAC_ELD_44100_2);
		} else {
			extradata = ASC_AAC_LC_44100_2;
			extradata_size = sizeof(ASC_AAC_LC_44100_2);
		}
		break;
	default:
		blog(LOG_ERROR, "[AirPlay] unsupported audio ct=%u", ct);
		return false;
	}

	if (!codec) {
		blog(LOG_ERROR, "[AirPlay] no decoder found for audio ct=%u", ct);
		return false;
	}

	AVCodecContext *ctx = avcodec_alloc_context3(codec);
	if (!ctx)
		return false;

	ctx->extradata = av_mallocz(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
	if (!ctx->extradata) {
		avcodec_free_context(&ctx);
		return false;
	}
	memcpy(ctx->extradata, extradata, extradata_size);
	ctx->extradata_size = (int)extradata_size;

	ctx->sample_rate = 44100;
	av_channel_layout_default(&ctx->ch_layout, 2);

	int ret = avcodec_open2(ctx, codec, NULL);
	if (ret < 0) {
		log_av_error("avcodec_open2 (audio)", ret);
		avcodec_free_context(&ctx);
		return false;
	}

	dec->ctx = ctx;
	blog(LOG_INFO, "[AirPlay] audio decoder opened: %s (ct=%u)",
	     codec->name, ct);
	return true;
}

bool audio_decoder_set_format(struct audio_decoder *dec, unsigned char ct)
{
	if (!dec)
		return false;
	if (dec->ct == ct && dec->ctx)
		return true;

	if (dec->ctx)
		avcodec_free_context(&dec->ctx);
	if (dec->swr) {
		swr_free(&dec->swr);
		dec->swr_in_rate = 0;
	}

	if (!open_codec(dec, ct)) {
		dec->ct = 0;
		return false;
	}

	dec->ct = ct;
	dec->err_count = 0;
	return true;
}

static bool ensure_swr(struct audio_decoder *dec, const AVFrame *frame)
{
	int channels = frame->ch_layout.nb_channels;

	if (dec->swr && dec->swr_in_rate == frame->sample_rate &&
	    dec->swr_in_channels == channels &&
	    dec->swr_in_fmt == frame->format)
		return true;

	if (dec->swr)
		swr_free(&dec->swr);

	AVChannelLayout out_layout;
	av_channel_layout_default(&out_layout, channels);

	int ret = swr_alloc_set_opts2(&dec->swr,
				      &out_layout,
				      AV_SAMPLE_FMT_FLT,
				      frame->sample_rate,
				      &frame->ch_layout,
				      (enum AVSampleFormat)frame->format,
				      frame->sample_rate,
				      0, NULL);
	if (ret < 0 || !dec->swr) {
		log_av_error("swr_alloc_set_opts2", ret);
		return false;
	}

	ret = swr_init(dec->swr);
	if (ret < 0) {
		log_av_error("swr_init", ret);
		swr_free(&dec->swr);
		return false;
	}

	dec->swr_in_rate = frame->sample_rate;
	dec->swr_in_channels = channels;
	dec->swr_in_fmt = frame->format;
	return true;
}

bool audio_decoder_decode(struct audio_decoder *dec, const uint8_t *aac_data,
			  size_t aac_size, uint64_t pts,
			  struct decoded_audio *out)
{
	if (!dec || !dec->ctx)
		return false;

	dec->pkt->data = (uint8_t *)aac_data;
	dec->pkt->size = (int)aac_size;
	dec->pkt->pts = (int64_t)pts;

	int ret = avcodec_send_packet(dec->ctx, dec->pkt);
	if (ret < 0) {
		if (dec->err_count < 10) {
			dec->err_count++;
			log_av_error("avcodec_send_packet (audio)", ret);
		}
		return false;
	}

	ret = avcodec_receive_frame(dec->ctx, dec->frame);
	if (ret < 0) {
		if (ret != AVERROR(EAGAIN) && dec->err_count < 10) {
			dec->err_count++;
			log_av_error("avcodec_receive_frame (audio)", ret);
		}
		return false;
	}

	int channels = dec->frame->ch_layout.nb_channels;
	int samples = dec->frame->nb_samples;

	if (!ensure_swr(dec, dec->frame))
		return false;

	/* Ensure output buffer is large enough */
	size_t needed = (size_t)samples * channels * sizeof(float);
	if (needed > dec->pcm_buf_size) {
		float *tmp = realloc(dec->pcm_buf, needed);
		if (!tmp)
			return false;
		dec->pcm_buf = tmp;
		dec->pcm_buf_size = needed;
	}

	/* Convert to interleaved float32 */
	uint8_t *out_buf = (uint8_t *)dec->pcm_buf;
	int converted = swr_convert(dec->swr, &out_buf, samples,
				    (const uint8_t **)dec->frame->data,
				    samples);
	if (converted <= 0) {
		if (dec->err_count < 10) {
			dec->err_count++;
			log_av_error("swr_convert (audio)", converted);
		}
		return false;
	}

	out->data = (uint8_t *)dec->pcm_buf;
	out->data_size = (size_t)converted * channels * sizeof(float);
	out->samples = converted;
	out->channels = channels;
	out->sample_rate = dec->frame->sample_rate;
	out->pts = dec->frame->pts;

	return true;
}
