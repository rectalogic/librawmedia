#ifndef RM_RAWMEDIA_H
#define RM_RAWMEDIA_H

#include <libavformat/avformat.h>

typedef struct RawMediaDecoder RawMediaDecoder;

void rawmedia_init();
RawMediaDecoder* rawmedia_create_decoder(const char* filename, AVRational framerate, uint32_t start_frame);
int rawmedia_decode_video(RawMediaDecoder* rmd);
int rawmedia_decode_audio(RawMediaDecoder* rmd);
void rawmedia_destroy_decoder(RawMediaDecoder* rmd);

#endif
