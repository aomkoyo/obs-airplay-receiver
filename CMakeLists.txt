cmake_minimum_required(VERSION 3.16)
project(obs-airplay-receiver VERSION 2.1.0 LANGUAGES C)

# --- Paths ---
set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")

# UxPlay built libs
set(UXPLAY_BUILD_DIR "${DEPS_DIR}/uxplay-build/build")
set(UXPLAY_LIB_DIR "${DEPS_DIR}/UxPlay/lib")

# OBS headers
set(OBS_SRC_DIR "${DEPS_DIR}/obs-sdk/obs-studio-32.1.1")

# FFmpeg headers (matching OBS's FFmpeg 7.x)
set(FFMPEG_INCLUDE_DIR "${DEPS_DIR}/ffmpeg7-include")
set(FFMPEG_LIB_DIR "${DEPS_DIR}/obs-ffmpeg-libs")

# OpenSSL
if(NOT DEFINED OPENSSL_DIR)
    set(OPENSSL_DIR "C:/Users/kitti/scoop/apps/openssl/current")
endif()

# Find OpenSSL libcrypto
find_library(OPENSSL_CRYPTO_LIB NAMES libcrypto crypto libcrypto-3-x64
    PATHS
        "${OPENSSL_DIR}/lib"
        "${OPENSSL_DIR}/lib/VC"
        "${OPENSSL_DIR}/lib/VC/x64"
        "${OPENSSL_DIR}/lib/VC/x64/MD"
        "${OPENSSL_DIR}/lib/VC/x64/MT"
        "${OPENSSL_DIR}/lib/VC/x86_64"
    NO_DEFAULT_PATH)
if(NOT OPENSSL_CRYPTO_LIB)
    find_library(OPENSSL_CRYPTO_LIB NAMES libcrypto crypto)
endif()
message(STATUS "OpenSSL crypto lib: ${OPENSSL_CRYPTO_LIB}")

# libplist
set(LIBPLIST_DIR "${DEPS_DIR}/libplist-2.7.0")

# --- Plugin shared library ---
add_library(obs-airplay-receiver MODULE
    src/plugin-main.c
    src/airplay-source.c
    src/video-decoder.c
    src/audio-decoder.c
)

# --- Include directories ---
target_include_directories(obs-airplay-receiver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    # OBS headers
    ${OBS_SRC_DIR}/libobs
    ${OBS_SRC_DIR}/deps/w32-pthreads
    # UxPlay headers (raop.h, dnssd.h, stream.h, logger.h)
    ${UXPLAY_LIB_DIR}
    ${UXPLAY_LIB_DIR}/playfair
    ${UXPLAY_LIB_DIR}/llhttp
    # libplist headers
    ${LIBPLIST_DIR}/include
    # FFmpeg headers
    ${FFMPEG_INCLUDE_DIR}
    # OpenSSL headers
    ${OPENSSL_DIR}/include
)

# --- Link libraries ---
# UxPlay static libs
target_link_libraries(obs-airplay-receiver PRIVATE
    ${UXPLAY_BUILD_DIR}/airplay.lib
    ${UXPLAY_BUILD_DIR}/playfair.lib
    ${UXPLAY_BUILD_DIR}/llhttp.lib
)

# libplist
target_link_libraries(obs-airplay-receiver PRIVATE
    ${LIBPLIST_DIR}/build/plist.lib
)

# OBS
target_link_libraries(obs-airplay-receiver PRIVATE ${DEPS_DIR}/obs.lib)

# OpenSSL
target_link_libraries(obs-airplay-receiver PRIVATE
    ${OPENSSL_CRYPTO_LIB}
)

# FFmpeg
target_link_directories(obs-airplay-receiver PRIVATE ${FFMPEG_LIB_DIR})
target_link_libraries(obs-airplay-receiver PRIVATE
    avcodec avutil swscale swresample
)

# Windows (dnssd.dll is loaded dynamically by UxPlay's dnssd.c at runtime)
target_link_libraries(obs-airplay-receiver PRIVATE
    ws2_32 iphlpapi
    ${DEPS_DIR}/w32-pthreads.lib
)

target_compile_definitions(obs-airplay-receiver PRIVATE
    WIN32
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    LIBPLIST_STATIC
    PLUGIN_VERSION="${PROJECT_VERSION}"
)

set_target_properties(obs-airplay-receiver PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    PREFIX ""
)

# Install
set(OBS_PLUGIN_DEST "$ENV{PROGRAMDATA}/obs-studio/plugins/obs-airplay-receiver/bin/64bit")
install(TARGETS obs-airplay-receiver DESTINATION ${OBS_PLUGIN_DEST})
