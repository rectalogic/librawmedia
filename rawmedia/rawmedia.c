#include "rawmedia.h"
#include "rawmedia_internal.h"
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavutil/log.h>

void rawmedia_init() {
    av_register_all();
    avfilter_register_all();
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
}

int rawmedia_init_session(RawMediaSession* session) {
    if (session->framerate_den <= 0 || session->framerate_num <= 0) {
        av_log(NULL, AV_LOG_FATAL, "Invalid framerate requested\n");
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

static void (*s_user_log_callback)(const char*) = NULL;
static int s_log_level = AV_LOG_INFO;

static void sanitize(uint8_t *line){
    while (*line) {
        if (*line < 0x08 || (*line > 0x0D && *line < 0x20))
            *line='?';
        line++;
    }
}

static void redirector_log_callback(void* ptr, int level, const char* fmt, va_list vl) {
    static int print_prefix = 1;
    static int count;
    static char prev[1024];
    char line[1024];

    if (level > s_log_level)
        return;
    av_log_format_line(ptr, level, fmt, vl, line, sizeof(line), &print_prefix);

    // Implement AV_LOG_SKIP_REPEATED
    if (print_prefix && !strcmp(line, prev)){
        count++;
        return;
    }
    if (count > 0) {
        snprintf(line, sizeof(line), "    Last message repeated %d times\n", count);
        s_user_log_callback(line);
        count = 0;
    }
    strcpy(prev, line);
    sanitize((uint8_t*)line);
    s_user_log_callback(line);
}

void rawmedia_set_log(int level, void (*callback)(const char*)) {
    s_log_level = level;
    av_log_set_level(level);
    s_user_log_callback = callback;
    av_log_set_callback(callback ? redirector_log_callback : av_log_default_callback);
}

