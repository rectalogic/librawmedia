#include "rawmedia.h"
#include "decoder.h"

int main(int argc, const char *argv[]) {
    rawmedia_init();
    RawMediaDecoderConfig config = { .framerate = (AVRational){30,1},
                                     .start_frame = 15 };
    RawMediaDecoder* rmd = rawmedia_create_decoder(argv[1], &config);
    if (!rmd)
        return -1;
    RawMediaDecoderInfo info;
    rawmedia_get_decoder_info(rmd, &info);

    uint8_t video_buffer[info.video_framebuffer_size];
    uint8_t audio_buffer[info.audio_framebuffer_size];
    rawmedia_decode_video(rmd, video_buffer);
    rawmedia_decode_audio(rmd, audio_buffer);

    rawmedia_destroy_decoder(rmd);
    return 0;
}
