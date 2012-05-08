#include "rawmedia.h"
#include "rawmedia_internal.h"
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>

void rawmedia_init() {
    av_register_all();
    avfilter_register_all();
}

int rawmedia_init_session(RawMediaSession* session) {
    if (session->framerate_den <= 0 || session->framerate_num <= 0
        || session->width <= 0 || session->height <= 0
        || session->width % 2) {
        av_log(NULL, AV_LOG_FATAL, "Invalid size/framerate requested\n");
        return -1;
    }

    int nb_channels = av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
    AVRational time_base = {session->framerate_den, session->framerate_num};
    int output_samples_per_frame =
        av_rescale_q(1, time_base, RAWMEDIA_AUDIO_TIME_BASE);

    int size = av_samples_get_buffer_size(NULL, nb_channels,
                                          output_samples_per_frame,
                                          RAWMEDIA_AUDIO_SAMPLE_FMT, 1);
    if (size <= 0)
        return -1;
    session->audio_framebuffer_size = size;
    return 0;
}


