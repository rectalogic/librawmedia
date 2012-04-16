#include "rawmedia.h"
#include "decoder.h"

int main(int argc, const char *argv[]) {
    rawmedia_init();
    RawMediaDecoderConfig config = { .framerate = (AVRational){30,1},
                                     .start_frame = 15 };
    RawMediaDecoder* rmd = rawmedia_create_decoder(argv[1], &config);
    RawMediaDecoderInfo info;
    rawmedia_get_decoder_info(rmd, &info);
    rawmedia_decode_video(rmd);
    rawmedia_decode_audio(rmd);
    rawmedia_destroy_decoder(rmd);
    return 0;
}
