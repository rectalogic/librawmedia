#ifndef RM_RAWMEDIA_DECODER_H
#define RM_RAWMEDIA_DECODER_H

//XXX don't use libav stuff in this header, public
#include <libavformat/avformat.h>

typedef struct RawMediaDecoder RawMediaDecoder;

typedef struct RawMediaDecoderConfig {
    AVRational framerate;
    uint32_t start_frame;

    int discard_video;
    //XXX video bounding box that we scale to meet?

    int discard_audio;
} RawMediaDecoderConfig;

typedef struct RawMediaDecoderInfo {
    uint32_t duration;                  // Duration in frames (max duration of audio and video), reduced by start_frame

    int has_video;
    uint32_t video_framebuffer_size;    // Frame buffer size in bytes
    uint32_t video_width;
    uint32_t video_height;

    int has_audio;
    uint32_t audio_framebuffer_size;    // Frame buffer size in bytes
} RawMediaDecoderInfo;

RawMediaDecoder* rawmedia_create_decoder(const char* filename, const RawMediaDecoderConfig* config);
// Must be called before beginning to decode
int rawmedia_get_decoder_info(const RawMediaDecoder* rmd, RawMediaDecoderInfo* info);
// output must be the size indicated in RawMediaDecoderInfo
int rawmedia_decode_video(RawMediaDecoder* rmd, uint8_t* output);
int rawmedia_decode_audio(RawMediaDecoder* rmd);
void rawmedia_destroy_decoder(RawMediaDecoder* rmd);

#endif
