#include "rawmedia.h"
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>

void rawmedia_init() {
    av_register_all();
    avfilter_register_all();
}


