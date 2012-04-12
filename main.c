#include "rawmedia.h"

int main(int argc, const char *argv[]) {
    rawmedia_init();
    RawMediaDecoder* rmd = rawmedia_create_decoder(argv[1], (AVRational){30,1}, 0);
    rawmedia_decode_video(rmd);
    rawmedia_decode_audio(rmd);
    rawmedia_destroy_decoder(rmd);
    return 0;
}
