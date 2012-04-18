#include "rawmedia.h"
#include <stdio.h>

#define FRAME_RATE_NUM 30
#define FRAME_RATE_DEN 1
#define START_FRAME 15
#define OUTPUT_FRAMES 300

int main(int argc, const char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input-file> <audio-out-file> <video-out-file>\n", argv[0]);
        return -1;
    }
    const char* filename = argv[1];
    const char* audio_filename = argv[2];
    const char* video_filename = argv[3];

    rawmedia_init();
    RawMediaDecoderConfig config = { .framerate_num = FRAME_RATE_NUM,
                                     .framerate_den = FRAME_RATE_DEN,
                                     .start_frame = START_FRAME };
    RawMediaDecoder* rmd = rawmedia_create_decoder(filename, &config);
    if (!rmd)
        return -1;

    FILE* audio_fp = fopen(audio_filename, "w");
    FILE* video_fp = fopen(video_filename, "w");
    if (!audio_fp || !video_fp) {
        fprintf(stderr, "Failed to open output files.\n");
        return -1;
    }

    RawMediaDecoderInfo info;
    if (rawmedia_get_decoder_info(rmd, &info) < 0) {
        fprintf(stderr, "Failed to get decoder info.\n");
        return -1;
    }

    uint8_t video_buffer[info.video_framebuffer_size];
    uint8_t audio_buffer[info.audio_framebuffer_size];
    for (int frame = 0; frame < OUTPUT_FRAMES; frame++) {
        if (rawmedia_decode_video(rmd, video_buffer) < 0) {
            fprintf(stderr, "Failed to decode video.\n");
            return -1;
        }
        if (rawmedia_decode_audio(rmd, audio_buffer) < 0) {
            fprintf(stderr, "Failed to decode audio.\n");
            return -1;
        }
        if (fwrite(video_buffer, sizeof(video_buffer), 1, video_fp) < 1) {
            fprintf(stderr, "Failed to write video.\n");
            return -1;
        }
        if (fwrite(audio_buffer, sizeof(audio_buffer), 1, audio_fp) < 1) {
            fprintf(stderr, "Failed to write audio.\n");
            return -1;
        }
    }

    rawmedia_destroy_decoder(rmd);
    fclose(video_fp);
    fclose(audio_fp);
    return 0;
}
