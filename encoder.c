#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/intreadwrite.h>
#include "rawmedia.h"
#include "rawmedia_internal.h"

struct RawMediaEncoder {
    AVFormatContext* format_ctx;

    struct RawMediaVideo {
        AVStream* avstream;
        AVFrame* avframe;
        int width;
        int height;
        int pts_per_frame;
        struct SwsContext* sws_ctx;
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
    codec_ctx->codec_id = RAWMEDIA_VIDEO_CODEC;
    codec_ctx->width = config->width;
    codec_ctx->height = config->height;
    codec_ctx->time_base.num = config->framerate_den;
    codec_ctx->time_base.den = config->framerate_num;
    codec_ctx->pix_fmt = RAWMEDIA_VIDEO_ENCODE_PIXEL_FORMAT;
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    codec_ctx->codec_tag = AV_RL32(RAWMEDIA_VIDEO_ENCODE_CODEC_TAG);

    if (avcodec_open2(avstream->codec, codec, NULL) < 0)
        return NULL;

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
    AVCodecContext* codec_ctx = avstream->codec;
    codec_ctx->sample_fmt = RAWMEDIA_AUDIO_SAMPLE_FMT;
    codec_ctx->sample_rate = RAWMEDIA_AUDIO_SAMPLE_RATE;
    codec_ctx->channels = av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(avstream->codec, codec, NULL) < 0)
        return NULL;

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

    if (!config->has_video && !config->has_audio)
        return NULL;

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
        rme->video.avstream = add_video_stream(format_ctx, config);
        if (!rme->video.avstream) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to create video stream.\n",
                   filename);
            goto error;
        }
        rme->video.width = config->width;
        rme->video.height = config->height;
        rme->video.pts_per_frame = config->framerate_den;
        if (!(rme->video.avframe = avcodec_alloc_frame()))
            goto error;
        if (avpicture_alloc((AVPicture*)rme->video.avframe,
                            RAWMEDIA_VIDEO_ENCODE_PIXEL_FORMAT,
                            rme->video.width, rme->video.height) < 0)
            goto error;
        rme->video.avframe->pts = 0;
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
        if (!(rme->audio.avframe = avcodec_alloc_frame()))
            goto error;
        rme->audio.avframe->pts = 0;
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

    if (init_encoder_info(rme, config) < 0)
        goto error;

    return rme;

error:
    rawmedia_destroy_encoder(rme);
    return NULL;
}

void rawmedia_destroy_encoder(RawMediaEncoder* rme) {
    //XXX need to send NULL frame to flush for DELAY encoders
    if (rme) {
        AVFormatContext* format_ctx = rme->format_ctx;
        if (format_ctx) {
            av_write_trailer(format_ctx);

            // Close codecs
            if (rme->video.avstream) {
                avcodec_close(rme->video.avstream->codec);
                avpicture_free((AVPicture*)rme->video.avframe);
                av_free(rme->video.avframe);
                sws_freeContext(rme->video.sws_ctx);
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

// Convert from decoder pixel format to encoder
static int convert_video(RawMediaEncoder* rme, uint8_t* input) {
    struct RawMediaVideo* video = &rme->video;
    video->sws_ctx =
        sws_getCachedContext(video->sws_ctx,
                             video->width, video->height,
                             RAWMEDIA_VIDEO_DECODE_PIXEL_FORMAT,
                             video->width, video->height,
                             RAWMEDIA_VIDEO_ENCODE_PIXEL_FORMAT,
                             SWS_BICUBIC,
                             NULL, NULL, NULL);
    if (!video->sws_ctx)
        return -1;
    AVPicture input_picture;
    avpicture_fill(&input_picture, input, RAWMEDIA_VIDEO_DECODE_PIXEL_FORMAT,
                   video->width, video->height);
    sws_scale(video->sws_ctx,
              (const uint8_t* const*)input_picture.data, input_picture.linesize,
              0, video->height,
              video->avframe->data, video->avframe->linesize);
    return 0;
}

int rawmedia_encode_video(RawMediaEncoder* rme, uint8_t* input) {
    int r = 0;
    struct RawMediaVideo* video = &rme->video;
    AVCodecContext* codec_ctx = video->avstream->codec;
    AVPacket pkt = {0};

    if ((r = convert_video(rme, input)) < 0)
        return r;

    int got_packet = 0;
    if ((r = avcodec_encode_video2(codec_ctx, &pkt, video->avframe, &got_packet)) < 0)
        return r;
    if (!got_packet)
        return 0;
    pkt.stream_index = video->avstream->index;
    if ((r = av_interleaved_write_frame(rme->format_ctx, &pkt)) < 0)
        return r;

    video->avframe->pts += video->pts_per_frame;

    return r;
}

int rawmedia_encode_audio(RawMediaEncoder* rme, uint8_t* input) {
    int r = 0;
    struct RawMediaAudio* audio = &rme->audio;
    AVPacket pkt = {0};

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

    audio->avframe->pts += audio->input_samples_per_frame;

    return r;
}

