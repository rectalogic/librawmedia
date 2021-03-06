// Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/intreadwrite.h>
#include "rawmedia.h"
#include "rawmedia_internal.h"

struct RawMediaEncoder {
    AVFormatContext* format_ctx;

    struct RawMediaVideo {
        AVStream* avstream;
        AVFrame* avframe;
        int min_framebuffer_size;
    } video;

    struct RawMediaAudio {
        AVStream* avstream;
        AVFrame* avframe;
        int framebuffer_size;
    } audio;
};

static AVStream* add_video_stream(AVFormatContext* format_ctx, const RawMediaSession* session, const RawMediaEncoderConfig* config) {
    AVStream* avstream = NULL;
    AVCodec* codec = avcodec_find_encoder(RAWMEDIA_VIDEO_CODEC);
    if (!codec)
        return NULL;
    avstream = avformat_new_stream(format_ctx, codec);
    if (!avstream)
        return NULL;
    AVCodecContext* codec_ctx = avstream->codec;
    codec_ctx->codec_id = RAWMEDIA_VIDEO_CODEC;
    codec_ctx->width = config->width;
    codec_ctx->height = config->height;
    codec_ctx->time_base.num = session->framerate_den;
    codec_ctx->time_base.den = session->framerate_num;
    codec_ctx->pix_fmt = RAWMEDIA_VIDEO_PIXEL_FORMAT;
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    codec_ctx->codec_tag = AV_RL32(RAWMEDIA_VIDEO_ENCODE_CODEC_TAG);

    if (avcodec_open2(avstream->codec, codec, NULL) < 0)
        return NULL;

    return avstream;
}

static AVStream* add_audio_stream(AVFormatContext* format_ctx) {
    AVStream* avstream = NULL;
    AVCodec* codec = avcodec_find_encoder(RAWMEDIA_AUDIO_CODEC);
    if (!codec)
        return NULL;
    avstream = avformat_new_stream(format_ctx, codec);
    if (!avstream)
        return NULL;
    AVCodecContext* codec_ctx = avstream->codec;
    codec_ctx->sample_fmt = RAWMEDIA_AUDIO_SAMPLE_FMT;
    codec_ctx->sample_rate = RAWMEDIA_AUDIO_SAMPLE_RATE;
    codec_ctx->channel_layout = RAWMEDIA_AUDIO_CHANNEL_LAYOUT;
    codec_ctx->channels = av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(avstream->codec, codec, NULL) < 0)
        return NULL;

    return avstream;
}

RawMediaEncoder* rawmedia_create_encoder(const char* filename, const RawMediaSession* session, const RawMediaEncoderConfig* config) {
    int r = 0;
    if ((!config->has_video && !config->has_audio)
        || session->audio_framebuffer_size <= 0
        || (config->has_video && (config->width <= 0 || config->height <= 0
                                  || config->width % 2))) {
        return NULL;
    }

    RawMediaEncoder* rme = av_mallocz(sizeof(RawMediaEncoder));
    AVFormatContext* format_ctx = NULL;
    if (!rme)
        return NULL;

    if ((r = avformat_alloc_output_context2(&format_ctx, NULL,
                                            RAWMEDIA_ENCODE_FORMAT,
                                            filename)) < 0) {
        av_log(NULL, AV_LOG_FATAL, "%s: failed to open output file (%d).\n", filename, r);
        goto error;
    }
    rme->format_ctx = format_ctx;

    if (config->has_video) {
        rme->video.avstream = add_video_stream(format_ctx, session, config);
        if (!rme->video.avstream) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to create video stream.\n",
                   filename);
            goto error;
        }
        if (!(rme->video.avframe = avcodec_alloc_frame()))
            goto error;
        rme->video.avframe->pts = 0;
        if ((rme->video.min_framebuffer_size =
             avpicture_get_size(RAWMEDIA_VIDEO_PIXEL_FORMAT, config->width, config->height)) <= 0) {
            av_log(NULL, AV_LOG_FATAL, "%s: invalid frame size.\n", filename);
            goto error;
        }
    }

    if (config->has_audio) {
        rme->audio.avstream = add_audio_stream(format_ctx);
        if (!rme->audio.avstream) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to create audio stream.\n",
                   filename);
            goto error;
        }
        if (!(rme->audio.avframe = avcodec_alloc_frame()))
            goto error;
        rme->audio.avframe->pts = 0;
        AVRational time_base = (AVRational){session->framerate_den,
                                            session->framerate_num};
        rme->audio.avframe->nb_samples =
            av_rescale_q(1, time_base, RAWMEDIA_AUDIO_TIME_BASE);
        rme->audio.framebuffer_size = session->audio_framebuffer_size;
    }

    if (!(format_ctx->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to open output file.\n",
                   filename);
            goto error;
        }
    }

    if (avformat_write_header(format_ctx, NULL) < 0) {
        av_log(NULL, AV_LOG_FATAL, "%s: failed to write header.\n",
               filename);
        goto error;
    }

    return rme;

error:
    rawmedia_destroy_encoder(rme);
    return NULL;
}

int rawmedia_destroy_encoder(RawMediaEncoder* rme) {
    int r = 0;
    if (rme) {
        int rc;
        AVFormatContext* format_ctx = rme->format_ctx;
        if (format_ctx) {
            rc = av_write_trailer(format_ctx);
            r = r || rc;

            // Close codecs
            if (rme->video.avstream) {
                rc = avcodec_close(rme->video.avstream->codec);
                r = r || rc;
                avcodec_free_frame(&rme->video.avframe);
            }
            if (rme->audio.avstream) {
                rc = avcodec_close(rme->audio.avstream->codec);
                r = r || rc;
                avcodec_free_frame(&rme->audio.avframe);
            }
            // Close output file
            if (!(format_ctx->flags & AVFMT_NOFILE) && format_ctx->pb) {
                rc = avio_close(format_ctx->pb);
                r = r || rc;
            }

            avformat_free_context(format_ctx);
        }
        av_free(rme);
    }
    return r;
}

// input must be in RAWMEDIA_VIDEO_PIXEL_FORMAT
// inputsize is the size of input in bytes
int rawmedia_encode_video(RawMediaEncoder* rme, const uint8_t* input, int inputsize) {
    int r = 0;
    struct RawMediaVideo* video = &rme->video;
    AVCodecContext* codec_ctx = video->avstream->codec;
    AVPacket pkt = {0};

    if (inputsize < video->min_framebuffer_size)
        return -1;

    video->avframe->data[0] = (uint8_t*)input;
    video->avframe->linesize[0] = inputsize / codec_ctx->height;

    av_init_packet(&pkt);

    int got_packet = 0;
    if ((r = avcodec_encode_video2(codec_ctx, &pkt, video->avframe, &got_packet)) < 0)
        return r;
    if (!got_packet)
        return 0;
    if (pkt.pts != AV_NOPTS_VALUE) {
        pkt.pts = av_rescale_q(pkt.pts,
                               codec_ctx->time_base,
                               video->avstream->time_base);
    }
    if (pkt.dts != AV_NOPTS_VALUE) {
        pkt.dts = av_rescale_q(pkt.dts,
                               codec_ctx->time_base,
                               video->avstream->time_base);
    }

    pkt.stream_index = video->avstream->index;
    if ((r = av_interleaved_write_frame(rme->format_ctx, &pkt)) < 0)
        return r;

    video->avframe->pts += video->avstream->codec->time_base.num;

    video->avframe->data[0] = NULL;
    video->avframe->linesize[0] = 0;

    av_free_packet(&pkt);

    return r;
}

int rawmedia_encode_audio(RawMediaEncoder* rme, const uint8_t* input) {
    int r = 0;
    struct RawMediaAudio* audio = &rme->audio;
    AVPacket pkt = {0};

    int nb_channels = av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
    if ((r = avcodec_fill_audio_frame(audio->avframe, nb_channels,
                                      RAWMEDIA_AUDIO_SAMPLE_FMT, input,
                                      rme->audio.framebuffer_size, 1)))
        return r;

    int got_packet = 0;
    if ((r = avcodec_encode_audio2(audio->avstream->codec, &pkt,
                                   audio->avframe, &got_packet)) < 0)
        return r;
    if (!got_packet)
        return 0;
    pkt.stream_index = audio->avstream->index;
    if ((r = av_interleaved_write_frame(rme->format_ctx, &pkt)) < 0)
        return r;

    audio->avframe->pts += audio->avframe->nb_samples;

    return r;
}

