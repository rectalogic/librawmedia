#include "rawmedia.h"
#include <stdio.h>

#define FRAME_RATE_NUM 30
#define FRAME_RATE_DEN 1
#define START_FRAME 0
#define WIDTH 320
#define HEIGHT 240

int main(int argc, const char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input-file> <output-file.mov> <audio-raw-file> <video-raw-file>\n", argv[0]);
        return -1;
    }
    const char* filename = argv[1];
    const char* output_filename = argv[2];
    const char* audio_filename = argv[3];
    const char* video_filename = argv[4];

    rawmedia_init();

    RawMediaSession session = {
        .framerate_num = FRAME_RATE_NUM,
        .framerate_den = FRAME_RATE_DEN,
    };
    if (rawmedia_init_session(&session) < 0)
        return -1;
    RawMediaDecoderConfig dconfig = { .max_width = WIDTH,
                                      .max_height = HEIGHT,
                                      .volume = 1,
                                      .start_frame = START_FRAME };
    RawMediaDecoder* rmd = rawmedia_create_decoder(filename, &session, &dconfig);
    if (!rmd)
        return -1;


    FILE* audio_fp = fopen(audio_filename, "w");
    FILE* video_fp = fopen(video_filename, "w");
    if (!audio_fp || !video_fp) {
        fprintf(stderr, "Failed to open output files.\n");
        return -1;
    }

    const RawMediaDecoderInfo* info = rawmedia_get_decoder_info(rmd);

    RawMediaEncoderConfig econfig = { .has_video = info->has_video,
                                      .has_audio = info->has_audio };
    RawMediaEncoder* rme = NULL;

    uint8_t *video_buffer = NULL;
    uint8_t audio_buffer[session.audio_framebuffer_size];
    int width = 0, height = 0, video_buffer_size;
    for (int frame = 0; frame < info->duration; frame++) {
        if (info->has_video && rawmedia_decode_video(rmd, &video_buffer, &width, &height, &video_buffer_size) < 0) {
            fprintf(stderr, "Failed to decode video.\n");
            return -1;
        }
        if (info->has_audio && rawmedia_decode_audio(rmd, audio_buffer) < 0) {
            fprintf(stderr, "Failed to decode audio.\n");
            return -1;
        }

        if (!rme) {
            econfig.width = width;
            econfig.height = height;
            rme = rawmedia_create_encoder(output_filename, &session, &econfig);
            if (!rme)
                return -1;
        }

        if (econfig.has_video) {
            if (rawmedia_encode_video(rme, video_buffer, video_buffer_size) < 0) {
                fprintf(stderr, "Failed to encode video.\n");
                return -1;
            }
            if (fwrite(video_buffer, video_buffer_size, 1, video_fp) < 1) {
                fprintf(stderr, "Failed to write video.\n");
                return -1;
            }
        }
        if (econfig.has_audio) {
            if (rawmedia_encode_audio(rme, audio_buffer) < 0) {
                fprintf(stderr, "Failed to encode audio.\n");
                return -1;
            }
            if (fwrite(audio_buffer, sizeof(audio_buffer), 1, audio_fp) < 1) {
                fprintf(stderr, "Failed to write audio.\n");
                return -1;
            }
        }
    }

    rawmedia_destroy_decoder(rmd);
    rawmedia_destroy_encoder(rme);

    fclose(video_fp);
    fclose(audio_fp);
    return 0;
}
