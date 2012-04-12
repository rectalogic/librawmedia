#ifndef RM_RAWMEDIA_H
#define RM_RAWMEDIA_H

typedef struct RawMediaDecoder RawMediaDecoder;

void rawmedia_init();
RawMediaDecoder* rawmedia_create_decoder(const char* filename);
int rawmedia_decode_video_frame(RawMediaDecoder* rmd);
void rawmedia_destroy_decoder(RawMediaDecoder* rmd);

#endif
