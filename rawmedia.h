#ifndef RM_RAWMEDIA_H
#define RM_RAWMEDIA_H

#include <stdint.h>

void rawmedia_init();

typedef struct RawMediaDecoder RawMediaDecoder;

typedef struct RawMediaDecoderConfig {
    int framerate_num;
    int framerate_den;

    int start_frame;

    int discard_video;
    //XXX video bounding box that we scale to meet?

    int discard_audio;
} RawMediaDecoderConfig;

typedef struct RawMediaDecoderInfo {
    int duration;                  // Duration in frames (max duration of audio and video), reduced by start_frame

    int has_video;
    int video_framebuffer_size;    // Frame buffer size in bytes
    int video_width;
    int video_height;

    int has_audio;
    int audio_framebuffer_size;    // Frame buffer size in bytes
} RawMediaDecoderInfo;

RawMediaDecoder* rawmedia_create_decoder(const char* filename, const RawMediaDecoderConfig* config);
// Must be called before beginning to decode
int rawmedia_get_decoder_info(const RawMediaDecoder* rmd, RawMediaDecoderInfo* info);
// output must be the size indicated in RawMediaDecoderInfo
int rawmedia_decode_video(RawMediaDecoder* rmd, uint8_t* output);
int rawmedia_decode_audio(RawMediaDecoder* rmd, uint8_t* output);
void rawmedia_destroy_decoder(RawMediaDecoder* rmd);

#endif
