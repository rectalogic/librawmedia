#ifndef RM_RAWMEDIA_H
#define RM_RAWMEDIA_H

#include <stdint.h>
#include <stdbool.h>

void rawmedia_init();

typedef struct RawMediaDecoder RawMediaDecoder;

typedef struct RawMediaDecoderConfig {
    int framerate_num;
    int framerate_den;

    int start_frame;

    bool discard_video;
    //XXX video bounding box that we scale to meet?

    bool discard_audio;
} RawMediaDecoderConfig;

typedef struct RawMediaDecoderInfo {
    int duration;                  // Duration in frames (max duration of audio and video), reduced by start_frame

    bool has_video;
    int video_framebuffer_size;    // Frame buffer size in bytes
    int width;
    int height;

    bool has_audio;
    int audio_framebuffer_size;    // Frame buffer size in bytes
} RawMediaDecoderInfo;

typedef struct RawMediaEncoder RawMediaEncoder;

typedef struct RawMediaEncoderConfig {
    int framerate_num;
    int framerate_den;
    int width;
    int height;

    bool has_video;
    bool has_audio;
} RawMediaEncoderConfig;

typedef struct RawMediaEncoderInfo {
    int video_framebuffer_size;    // Frame buffer size in bytes
    int audio_framebuffer_size;    // Frame buffer size in bytes
} RawMediaEncoderInfo;

RawMediaDecoder* rawmedia_create_decoder(const char* filename, const RawMediaDecoderConfig* config);
const RawMediaDecoderInfo* rawmedia_get_decoder_info(const RawMediaDecoder* rmd);
// output must be the size indicated in RawMediaDecoderInfo
int rawmedia_decode_video(RawMediaDecoder* rmd, uint8_t* output);
int rawmedia_decode_audio(RawMediaDecoder* rmd, uint8_t* output);
void rawmedia_destroy_decoder(RawMediaDecoder* rmd);

RawMediaEncoder* rawmedia_create_encoder(const char* filename, const RawMediaEncoderConfig* config);
const RawMediaEncoderInfo* rawmedia_get_encoder_info(const RawMediaEncoder* rme);
int rawmedia_encode_video(RawMediaEncoder* rme, const uint8_t* input);
int rawmedia_encode_audio(RawMediaEncoder* rme, const uint8_t* input);
void rawmedia_destroy_encoder(RawMediaEncoder* rme);

#endif
