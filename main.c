#include "rawmedia.h"

int main(int argc, const char *argv[]) {
    rawmedia_init();
    RawMediaDecoder* rmd = rawmedia_create_decoder(argv[1]);
    rawmedia_decode_video_frame(rmd);
    rawmedia_destroy_decoder(rmd);
    return 0;
}
