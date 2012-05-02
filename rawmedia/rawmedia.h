#ifndef RM_RAWMEDIA_H
#define RM_RAWMEDIA_H

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


int rawmedia_init_session(RawMediaSession* session);

RawMediaDecoder* rawmedia_create_decoder(const char* filename, const RawMediaSession* session, const RawMediaDecoderConfig* config);
const RawMediaDecoderInfo* rawmedia_get_decoder_info(const RawMediaDecoder* rmd);
int rawmedia_decode_video(RawMediaDecoder* rmd, uint8_t** output, int* linesize);
// output must be the size indicated in RawMediaSession
int rawmedia_decode_audio(RawMediaDecoder* rmd, uint8_t* output);
void rawmedia_destroy_decoder(RawMediaDecoder* rmd);

RawMediaEncoder* rawmedia_create_encoder(const char* filename, const RawMediaSession* session, const RawMediaEncoderConfig* config);
int rawmedia_encode_video(RawMediaEncoder* rme, const uint8_t* input, int linesize);
// input must be the size indicated in RawMediaSession
int rawmedia_encode_audio(RawMediaEncoder* rme, const uint8_t* input);
void rawmedia_destroy_encoder(RawMediaEncoder* rme);

#endif
