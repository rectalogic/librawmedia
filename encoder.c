#include <libavformat/avformat.h>
#include "rawmedia.h"
#include "rawmedia_internal.h"

struct RawMediaEncoder {
    AVFormatContext* format_ctx;

    struct RawMediaVideo {
        AVStream* avstream;
    } video;

    struct RawMediaAudio {
        AVStream* avstream;
        AVFrame* avframe;
        int input_samples_per_frame;
    } audio;

    RawMediaEncoderInfo info;
};

static AVStream* add_video_stream(AVFormatContext* format_ctx, const RawMediaEncoderConfig* config) {
    AVStream* avstream = NULL;
    AVCodec* codec = avcodec_find_encoder(RAWMEDIA_VIDEO_CODEC);
    if (!codec)
        return NULL;
    avstream = avformat_new_stream(format_ctx, codec);
    if (!avstream)
        return NULL;
    AVCodecContext* codec_ctx = avstream->codec;
    if (avcodec_get_context_defaults3(codec_ctx, codec) < 0)
        return NULL;

    codec_ctx->codec_id = RAWMEDIA_VIDEO_CODEC;
    codec_ctx->width = config->width;
    codec_ctx->height = config->height;
    codec_ctx->time_base.num = config->framerate_den;
    codec_ctx->time_base.den = config->framerate_num;
    codec_ctx->pix_fmt = RAWMEDIA_VIDEO_ENCODE_PIXEL_FORMAT;
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return avstream;
}

static AVStream* add_audio_stream(AVFormatContext* format_ctx, const RawMediaEncoderConfig* config) {
    AVStream* avstream = NULL;
    AVCodec* codec = avcodec_find_encoder(RAWMEDIA_AUDIO_CODEC);
    if (!codec)
        return NULL;
    avstream = avformat_new_stream(format_ctx, codec);
    if (!avstream)
        return NULL;
    //XXX do we need to set stream->id = 1?
    AVCodecContext* codec_ctx = avstream->codec;
    if (avcodec_get_context_defaults3(codec_ctx, codec) < 0)
        return NULL;

    codec_ctx->sample_fmt = RAWMEDIA_AUDIO_SAMPLE_FMT;
    codec_ctx->sample_rate = RAWMEDIA_AUDIO_SAMPLE_RATE;
    codec_ctx->channels = av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return avstream;
}

static int init_encoder_info(RawMediaEncoder* rme, const RawMediaEncoderConfig* config) {
    RawMediaEncoderInfo* info = &rme->info;

    if (rme->video.avstream) {
        int size = avpicture_get_size(RAWMEDIA_VIDEO_DECODE_PIXEL_FORMAT,
                                      config->width, config->height);
        if (size <= 0)
            return -1;
        info->video_framebuffer_size = size;
    }

    if (rme->audio.avstream) {
        int nb_channels = av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
        int size = av_samples_get_buffer_size(NULL, nb_channels,
                                              rme->audio.input_samples_per_frame,
                                              RAWMEDIA_AUDIO_SAMPLE_FMT, 1);
        if (size <= 0)
            return -1;
        info->audio_framebuffer_size = size;
    }
    return 0;
}

RawMediaEncoder* rawmedia_create_encoder(const char* filename, const RawMediaEncoderConfig* config) {
    int r = 0;
    RawMediaEncoder* rme = av_mallocz(sizeof(RawMediaEncoder));
    AVFormatContext* format_ctx = NULL;
    if (!rme)
        return NULL;

    if ((r = avformat_alloc_output_context2(&format_ctx, NULL, "mov", filename)) < 0) {
        av_log(NULL, AV_LOG_FATAL, "%s: failed to open output file (%d).\n", filename, r);
        goto error;
    }
    rme->format_ctx = format_ctx;

    if (config->has_video) {
        rme->video.avstream = add_video_stream(format_ctx, config);
        if (!rme->video.avstream) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to create video stream.\n",
                   filename);
            goto error;
        }
    }
    if (config->has_audio) {
        rme->audio.avstream = add_audio_stream(format_ctx, config);
        if (!rme->audio.avstream) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to create audio stream.\n",
                   filename);
            goto error;
        }
        AVRational time_base = (AVRational){config->framerate_den,
                                            config->framerate_num};
        rme->audio.input_samples_per_frame =
            av_rescale_q(1, time_base, RAWMEDIA_AUDIO_TIME_BASE);
    }

    if (rme->video.avstream) {
        if (avcodec_open2(rme->video.avstream->codec, NULL, NULL) < 0) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to open video codec.\n",
                   filename);
            goto error;
        }
        //XXX see alloc_picture in sample - need a frame of the output pix_fmt
        //XXX we should use avcodec_encode_video2 which sample does not
    }
    if (rme->audio.avstream) {
        if (avcodec_open2(rme->audio.avstream->codec, NULL, NULL) < 0) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to open audio codec.\n",
                   filename);
            goto error;
        }
        if (!(rme->audio.avframe = avcodec_alloc_frame()))
            goto error;
    }

    if (!(format_ctx->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to open output file.\n",
                   filename);
            goto error;
        }
    }
    //XXX pass options with yuvs tag?
    if (avformat_write_header(format_ctx, NULL) < 0) {
        av_log(NULL, AV_LOG_FATAL, "%s: failed to write header.\n",
               filename);
        goto error;
    }

    if (init_encoder_info(rme, config) < 0)
        goto error;

    return rme;

error:
    rawmedia_destroy_encoder(rme);
    return NULL;
}

void rawmedia_destroy_encoder(RawMediaEncoder* rme) {
    if (rme) {
        AVFormatContext* format_ctx = rme->format_ctx;
        if (format_ctx) {
            av_write_trailer(format_ctx);

            // Close codecs
            if (rme->video.avstream) {
                avcodec_close(rme->video.avstream->codec);
                //XXX av_free picture/tmp_picture/video_outbuf? (see muxing.c)
            }
            if (rme->audio.avstream) {
                avcodec_close(rme->audio.avstream->codec);
                av_free(rme->audio.avframe);
            }
            // Close output file
            if (!(format_ctx->flags & AVFMT_NOFILE) && format_ctx->pb)
                avio_close(format_ctx->pb);

            avformat_free_context(format_ctx);
        }
        av_free(rme);
    }
}

const RawMediaEncoderInfo*  rawmedia_get_encoder_info(const RawMediaEncoder* rme) {
    return &rme->info;
}

int rawmedia_encode_video(RawMediaEncoder* rme, uint8_t* input) {
}

int rawmedia_encode_audio(RawMediaEncoder* rme, uint8_t* input) {
    int r = 0;
    struct RawMediaAudio* audio = &rme->audio;
    AVPacket pkt;
    av_init_packet(&pkt);
    audio->avframe->nb_samples = audio->input_samples_per_frame;
    int nb_channels = av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
    if ((r = avcodec_fill_audio_frame(audio->avframe, nb_channels,
                                      RAWMEDIA_AUDIO_SAMPLE_FMT, input,
                                      rme->info.audio_framebuffer_size, 1)))
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
    return r;
}

