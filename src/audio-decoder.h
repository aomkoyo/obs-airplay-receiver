#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct audio_decoder;

struct decoded_audio {
	uint8_t *data;          /* Interleaved float32 PCM data */
	size_t data_size;       /* Size in bytes */
	int samples;            /* Number of samples per channel */
	int channels;           /* Number of channels */
	int sample_rate;        /* Sample rate in Hz */
	uint64_t pts;
};

/* Create a new audio decoder (codec is chosen when the AirPlay
 * compression type is known - see audio_decoder_set_format). */
struct audio_decoder *audio_decoder_create(void);

/* Destroy the decoder */
void audio_decoder_destroy(struct audio_decoder *dec);

/* (Re)configure the decoder for an AirPlay compression type:
 * ct = 2 (ALAC), 4 (AAC-LC), 8 (AAC-ELD).
 * Cheap no-op when ct is unchanged. ct = 1 (PCM) needs no decoder. */
bool audio_decoder_set_format(struct audio_decoder *dec, unsigned char ct);

/* Decode one compressed packet into PCM float32.
 * Returns true if audio was produced. */
bool audio_decoder_decode(struct audio_decoder *dec, const uint8_t *aac_data,
			  size_t aac_size, uint64_t pts,
			  struct decoded_audio *out);
