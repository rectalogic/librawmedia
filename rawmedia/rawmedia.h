#ifndef RM_RAWMEDIA_H
#define RM_RAWMEDIA_H

#ifdef rawmedia_EXPORTS
    #include "exports.h"
#else
    #define RAWMEDIA_IMPORT
    #define RAWMEDIA_EXPORT
    #define RAWMEDIA_LOCAL
#endif
#include <stdint.h>
#include <stdbool.h>

typedef struct RawMediaSession {
    // Target framerate
    int framerate_num;
    int framerate_den;
    // Video will be scaled and padded to fit these bounds
    int width;
    int height;

    // This will be set to required audio buffer size after initialization.
    // This is the number of bytes returned on decode, and expected on encode.
    int audio_framebuffer_size;
} RawMediaSession;

typedef struct RawMediaDecoder RawMediaDecoder;

typedef struct RawMediaDecoderConfig {
    int start_frame;

    // Linear 0..1. Caller should convert from exponential.
    // See http://www.dr-lex.be/info-stuff/volumecontrols.html
    float volume;

    bool discard_video;
    bool discard_audio;
} RawMediaDecoderConfig;

typedef struct RawMediaDecoderInfo {
    int duration;                  // Duration in frames (max duration of audio and video), reduced by start_frame

    bool has_video;
    bool has_audio;
} RawMediaDecoderInfo;

typedef struct RawMediaEncoder RawMediaEncoder;

typedef struct RawMediaEncoderConfig {
    bool has_video;
    bool has_audio;
} RawMediaEncoderConfig;


RAWMEDIA_EXPORT int rawmedia_init_session(RawMediaSession* session);

RAWMEDIA_EXPORT RawMediaDecoder* rawmedia_create_decoder(const char* filename, const RawMediaSession* session, const RawMediaDecoderConfig* config);
RAWMEDIA_EXPORT const RawMediaDecoderInfo* rawmedia_get_decoder_info(const RawMediaDecoder* rmd);
RAWMEDIA_EXPORT int rawmedia_decode_video(RawMediaDecoder* rmd, uint8_t** output, int* linesize);
// output must be the size indicated in RawMediaSession
RAWMEDIA_EXPORT int rawmedia_decode_audio(RawMediaDecoder* rmd, uint8_t* output);
RAWMEDIA_EXPORT void rawmedia_destroy_decoder(RawMediaDecoder* rmd);

RAWMEDIA_EXPORT RawMediaEncoder* rawmedia_create_encoder(const char* filename, const RawMediaSession* session, const RawMediaEncoderConfig* config);
RAWMEDIA_EXPORT int rawmedia_encode_video(RawMediaEncoder* rme, const uint8_t* input, int linesize);
// input must be the size indicated in RawMediaSession
RAWMEDIA_EXPORT int rawmedia_encode_audio(RawMediaEncoder* rme, const uint8_t* input);
RAWMEDIA_EXPORT void rawmedia_destroy_encoder(RawMediaEncoder* rme);

#endif
