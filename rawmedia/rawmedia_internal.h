// Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#ifndef RM_RAWMEDIA_INTERNAL_H
#define RM_RAWMEDIA_INTERNAL_H

#include <libavcodec/avcodec.h>

// Formats decoded by decoder and expected by encoder

// Only use packed pixel formats
#define RAWMEDIA_VIDEO_PIXEL_FORMAT PIX_FMT_UYVY422
#define RAWMEDIA_AUDIO_SAMPLE_FMT AV_SAMPLE_FMT_S16
#define RAWMEDIA_AUDIO_DATATYPE int16_t
#define RAWMEDIA_AUDIO_SAMPLE_MIN -32768
#define RAWMEDIA_AUDIO_SAMPLE_MAX 32767
#define RAWMEDIA_AUDIO_SAMPLE_RATE 44100
#define RAWMEDIA_AUDIO_CHANNEL_LAYOUT AV_CH_LAYOUT_STEREO
#define RAWMEDIA_AUDIO_SILENCE 0
#define RAWMEDIA_AUDIO_TIME_BASE ((AVRational){1, RAWMEDIA_AUDIO_SAMPLE_RATE})

#define RAWMEDIA_VIDEO_CODEC CODEC_ID_RAWVIDEO
#define RAWMEDIA_AUDIO_CODEC CODEC_ID_PCM_S16LE
#define RAWMEDIA_VIDEO_ENCODE_CODEC_TAG "yuvs"
#define RAWMEDIA_ENCODE_FORMAT "mov"

#define INVALID_STREAM -1

#endif
