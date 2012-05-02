#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/asrc_abuffer.h>
#include <libswscale/swscale.h>
#include "rawmedia.h"
#include "rawmedia_internal.h"
#include "packet_queue.h"

enum StreamStatus {
    SS_EOF_PENDING = -1,
    SS_NORMAL = 0,
    SS_EOF = 1,
};

struct RawMediaDecoder {
    AVFormatContext* format_ctx;
    AVRational time_base;

    struct RawMediaVideo {
        int stream_index;
        PacketQueue packetq;
        AVFrame* avframe;
        uint32_t current_frame;     // Frame number we are decoding
        uint32_t frame_duration;    // Frame duration in video timebase
        AVFilterContext* buffersink_ctx;
        AVFilterContext* buffersrc_ctx;
        AVFilterGraph* filter_graph;
        AVFilterBufferRef* picref;
        enum StreamStatus status;
    } video;

    struct RawMediaAudio {
        int stream_index;
        PacketQueue packetq;
        AVFrame* avframe;
        int output_samples_per_frame;
        AVPacket pkt;
        AVPacket pkt_partial;
        AVFilterContext* abuffersink_ctx;
        AVFilterContext* abuffersrc_ctx;
        AVFilterGraph* filter_graph;
        AVFilterBufferRef* samplesref;
        int nb_samples_consumed; // Number of samples already consumed from samplesref
        enum StreamStatus status;
    } audio;

    RawMediaDecoderInfo info;
};

static int64_t video_expected_pts(RawMediaDecoder* rmd);
static inline int64_t video_frame_pts(AVFrame* avframe);
static int next_video_frame(RawMediaDecoder* rmd, int64_t expected_pts);
static int first_audio_frame(RawMediaDecoder* rmd, int64_t start_sample);

static inline AVStream* get_avstream(const RawMediaDecoder* rmd, int stream_index) {
    return rmd->format_ctx->streams[stream_index];
}

static int open_decoder(AVCodecContext* ctx, AVCodec* codec) {
    int r = 0;
    // Disable threading, introduces codec delay
    AVDictionary* opts = NULL;
    av_dict_set(&opts, "threads", "1", 0);
    r = avcodec_open2(ctx, codec, &opts);
    av_dict_free(&opts);
    return r;
}

static int init_video_filters(RawMediaDecoder* rmd, const RawMediaDecoderConfig* config) {
    int r = 0;
    char args[512];
    AVStream* stream = get_avstream(rmd, rmd->video.stream_index);
    AVCodecContext* video_ctx = stream->codec;
    AVFilterInOut* outputs = NULL;
    AVFilterInOut* inputs = NULL;
    static const enum PixelFormat pixel_fmts[] = { RAWMEDIA_VIDEO_PIXEL_FORMAT,
                                                   PIX_FMT_NONE };

    if (!(rmd->video.filter_graph = avfilter_graph_alloc())) {
        r = -1;
        goto error;
    }
    snprintf(args, sizeof(args), "%d:%d:%d:%d:%d:%d:%d:%d",
             video_ctx->width, video_ctx->height, video_ctx->pix_fmt,
             video_ctx->time_base.num, video_ctx->time_base.den,
             video_ctx->sample_aspect_ratio.num,
             video_ctx->sample_aspect_ratio.den, SWS_LANCZOS);
    if ((r = avfilter_graph_create_filter(&rmd->video.buffersrc_ctx,
                                          avfilter_get_by_name("buffer"),
                                          "in", args, NULL,
                                          rmd->video.filter_graph)) < 0)
        goto error;

#if FF_API_OLD_VSINK_API
    r = avfilter_graph_create_filter(&rmd->video.buffersink_ctx,
                                     avfilter_get_by_name("buffersink"),
                                     "out", NULL, (void*)pixel_fmts,
                                     rmd->video.filter_graph);
#else
    AVBufferSinkParams *buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pixel_fmts;
    r = avfilter_graph_create_filter(&rmd->video.buffersink_ctx,
                                     avfilter_get_by_name("buffersink"),
                                     "out", NULL, buffersink_params,
                                     rmd->video.filter_graph);
    av_freep(&buffersink_params);
#endif
    if (r < 0)
        goto error;

    inputs = avfilter_inout_alloc();
    outputs = avfilter_inout_alloc();
    if (!inputs || !outputs) {
        r = -1;
        goto error;
    }
    outputs->name = av_strdup("in");
    outputs->filter_ctx = rmd->video.buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = rmd->video.buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    // Scale to "meet" bounds, maintaining source aspect ratio, and without upscaling.
    // We specify both width and height, instead of using "-1" ("keep aspect")
    // because that just sets the aspect ratio and doesn't actually scale
    // the pixels.
    // After scaling, we center the image in the padded output bounds.
    // %1$d is width, %2$d is height
    snprintf(args, sizeof(args),
             "scale=trunc(st(0\\,iw*sar)*min(1\\,min(%1$d/ld(0)\\,%2$d/ih))+0.5):ow/dar+0.5,"
             "pad=max(%1$d\\,iw):max(%2$d\\,ih):max(0\\,(ow-iw)/2):max(0\\,(oh-ih)/2)",
             config->width, config->height);
    if ((r = avfilter_graph_parse(rmd->video.filter_graph, args,
                                  &inputs, &outputs, NULL)) < 0)
        goto error;
    if ((r = avfilter_graph_config(rmd->video.filter_graph, NULL)) < 0)
        goto error;

    return r;

error:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return r;
}

static int init_audio_filters(RawMediaDecoder* rmd, const RawMediaDecoderConfig* config) {
    int r = 0;
    char args[512];
    AVStream* stream = get_avstream(rmd, rmd->audio.stream_index);
    AVCodecContext* audio_ctx = stream->codec;
    AVFilterInOut* outputs = NULL;
    AVFilterInOut* inputs = NULL;
    static const enum AVSampleFormat sample_fmts[] = { RAWMEDIA_AUDIO_SAMPLE_FMT,
                                                       AV_SAMPLE_FMT_NONE };
    int packing_fmts[] =
        { av_sample_fmt_is_planar(RAWMEDIA_AUDIO_SAMPLE_FMT)
          ? AVFILTER_PLANAR
          : AVFILTER_PACKED,
          -1 };
    static const int64_t chlayouts[] = { RAWMEDIA_AUDIO_CHANNEL_LAYOUT, -1 };

    if (!(rmd->audio.filter_graph = avfilter_graph_alloc())) {
        r = -1;
        goto error;
    }

    if (!audio_ctx->channel_layout)
        audio_ctx->channel_layout = av_get_default_channel_layout(audio_ctx->channels);
    snprintf(args, sizeof(args), "%d:%d:0x%"PRIx64":%s",
             audio_ctx->sample_rate, audio_ctx->sample_fmt,
             audio_ctx->channel_layout,
             av_sample_fmt_is_planar(audio_ctx->sample_fmt) ? "planar" : "packed");
    if ((r = avfilter_graph_create_filter(&rmd->audio.abuffersrc_ctx,
                                          avfilter_get_by_name("abuffer"),
                                          "in", args, NULL,
                                          rmd->audio.filter_graph)) < 0)
        goto error;

    AVABufferSinkParams *abuffersink_params = av_abuffersink_params_alloc();
    abuffersink_params->sample_fmts = sample_fmts;
    abuffersink_params->channel_layouts = chlayouts;
    abuffersink_params->packing_fmts = packing_fmts;
    r = avfilter_graph_create_filter(&rmd->audio.abuffersink_ctx,
                                     avfilter_get_by_name("abuffersink"),
                                     "out", NULL, abuffersink_params,
                                     rmd->audio.filter_graph);
    av_freep(&abuffersink_params);
    if (r < 0)
        goto error;

    inputs = avfilter_inout_alloc();
    outputs = avfilter_inout_alloc();
    if (!inputs || !outputs) {
        r = -1;
        goto error;
    }
    outputs->name = av_strdup("in");
    outputs->filter_ctx = rmd->audio.abuffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = rmd->audio.abuffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    int length = snprintf(args, sizeof(args), "aconvert,aresample=%d",
                          RAWMEDIA_AUDIO_SAMPLE_RATE);
    if (config->volume < 1) {
        snprintf(&args[length], sizeof(args) - length, ",volume=%f",
                 config->volume);
    }
    if ((r = avfilter_graph_parse(rmd->audio.filter_graph, args,
                                  &inputs, &outputs, NULL)) < 0)
        goto error;
    if ((r = avfilter_graph_config(rmd->audio.filter_graph, NULL)) < 0)
        goto error;

    return r;

error:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return r;
}

static int initial_seek(RawMediaDecoder* rmd, int start_frame, const char* filename) {
    int r = 0;
    if (start_frame <= 0)
        return 0;

    int64_t timestamp = av_rescale_q(start_frame, rmd->time_base, AV_TIME_BASE_Q);
    if (av_seek_frame(rmd->format_ctx, -1, timestamp, AVSEEK_FLAG_BACKWARD) < 0)
        av_log(NULL, AV_LOG_WARNING, "%s: failed to seek\n", filename);

    int64_t video_pts = -1;
    // Skip frames in video seeking forward
    if (rmd->video.stream_index != INVALID_STREAM) {
        video_pts = video_expected_pts(rmd);
        if ((r = next_video_frame(rmd, video_pts)) < 0) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to seek video\n", filename);
            return r;
        }
        // Save actual pts of frame we used, to compute audio offset below.
        // Subtract start_time for cases where video pts does not start at 0
        video_pts = video_frame_pts(rmd->video.avframe);
        AVStream* stream = get_avstream(rmd, rmd->video.stream_index);
        if (stream->start_time != AV_NOPTS_VALUE)
            video_pts -= stream->start_time;
    }

    // Skip frames in audio seeking forward
    if (rmd->audio.stream_index != INVALID_STREAM) {
        AVRational audio_time_base = get_avstream(rmd, rmd->audio.stream_index)->time_base;
        AVRational video_time_base = get_avstream(rmd, rmd->video.stream_index)->time_base;
        int64_t first_audio_sample = 0;
        if (video_pts != -1) {
            first_audio_sample = av_rescale_q(video_pts, video_time_base,
                                              audio_time_base);
        }
        else {
            first_audio_sample = av_rescale_q(start_frame, rmd->time_base,
                                              audio_time_base);
        }
        if ((r = first_audio_frame(rmd, first_audio_sample)) < 0) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to seek audio\n", filename);
            return r;
        }
    }
    return r;
}

// Stream duration in output frames
static int64_t output_stream_duration(const RawMediaDecoder* rmd, int stream, int start_frame) {
    AVStream* avstream = get_avstream(rmd, stream);
    int64_t frame_duration = av_rescale_q(1, rmd->time_base, avstream->time_base);
    int64_t duration = avstream->duration;
    int64_t frames = duration / frame_duration;
    // Round up if partial frame
    if (duration % frame_duration)
        frames++;
    frames -= start_frame;
    return frames;
}

static int init_decoder_info(RawMediaDecoder* rmd, const RawMediaDecoderConfig* config) {
    RawMediaDecoderInfo* info = &rmd->info;

    if (rmd->video.stream_index != INVALID_STREAM) {
        info->has_video = true;
        int64_t duration = output_stream_duration(rmd, rmd->video.stream_index,
                                                  config->start_frame);
        if (info->duration < duration)
            info->duration = duration;
    }

    if (rmd->audio.stream_index != INVALID_STREAM) {
        info->has_audio = true;
        int64_t duration = output_stream_duration(rmd, rmd->audio.stream_index,
                                                  config->start_frame);
        if (info->duration < duration)
            info->duration = duration;

        int nb_channels = av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
        int size = av_samples_get_buffer_size(NULL, nb_channels,
                                              rmd->audio.output_samples_per_frame,
                                              RAWMEDIA_AUDIO_SAMPLE_FMT, 1);
        if (size <= 0)
            return -1;
        info->audio_framebuffer_size = size;
    }
    return 0;
}

RawMediaDecoder* rawmedia_create_decoder(const char* filename, const RawMediaDecoderConfig* config) {
    if (config->framerate_den <= 0 || config->framerate_num <= 0
        || config->width <= 0 || config->height <= 0) {
        av_log(NULL, AV_LOG_FATAL,
               "%s: invalid size/framerate requested\n", filename);
        return NULL;
    }
    int r = 0;
    RawMediaDecoder* rmd = av_mallocz(sizeof(RawMediaDecoder));
    AVFormatContext* format_ctx = NULL;
    if (!rmd)
        return NULL;

    rmd->time_base = (AVRational){config->framerate_den, config->framerate_num};

    if ((r = avformat_open_input(&format_ctx, filename, NULL, NULL)) != 0) {
        av_log(NULL, AV_LOG_FATAL,
               "%s: failed to open (%d)\n", filename, r);
        goto error;
    }
    rmd->format_ctx = format_ctx;

    if ((r = avformat_find_stream_info(format_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_FATAL,
               "%s: failed to find stream info (%d)\n", filename, r);
        goto error;
    }

    if (!config->discard_video) {
        AVCodec* video_decoder = NULL;
        r = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1,
                                &video_decoder, 0);
        if (r >= 0) {
            rmd->video.stream_index = r;
            AVStream* stream = get_avstream(rmd, rmd->video.stream_index);
            if ((r = open_decoder(stream->codec, video_decoder)) < 0) {
                av_log(NULL, AV_LOG_FATAL,
                       "%s: failed to open video decoder\n", filename);
                goto error;
            }
            if (!(rmd->video.avframe = avcodec_alloc_frame()))
                goto error;
            rmd->video.frame_duration = av_rescale_q(1, rmd->time_base,
                                                     stream->time_base);
            rmd->video.current_frame = config->start_frame;
            if ((r = init_video_filters(rmd, config)) < 0)
                goto error;
        }
        else if (r == AVERROR_STREAM_NOT_FOUND
                 || r == AVERROR_DECODER_NOT_FOUND)
            rmd->video.stream_index = INVALID_STREAM;
        else
            goto error;
    }
    else
        rmd->video.stream_index = INVALID_STREAM;

    if (!config->discard_audio && config->volume > 0) {
        AVCodec* audio_decoder = NULL;
        r = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1,
                                &audio_decoder, 0);
        if (r >= 0) {
            rmd->audio.stream_index = r;
            AVStream* stream = get_avstream(rmd, rmd->audio.stream_index);
            if ((r = open_decoder(stream->codec, audio_decoder)) < 0) {
                av_log(NULL, AV_LOG_FATAL,
                       "%s: failed to open audio decoder\n", filename);
                goto error;
            }
            if (!(rmd->audio.avframe = avcodec_alloc_frame()))
                goto error;
            // How many samples per frame at our target framerate/samplerate
            rmd->audio.output_samples_per_frame =
                av_rescale_q(1, rmd->time_base, RAWMEDIA_AUDIO_TIME_BASE);

            if ((r = init_audio_filters(rmd, config)) < 0)
                goto error;
        }
        else if (r == AVERROR_STREAM_NOT_FOUND
                 || r == AVERROR_DECODER_NOT_FOUND)
            rmd->audio.stream_index = INVALID_STREAM;
        else
            goto error;
    }
    else
        rmd->audio.stream_index = INVALID_STREAM;

    // Discard other streams
    for (int stream_index = 0; stream_index < format_ctx->nb_streams; stream_index++) {
        if (stream_index != rmd->video.stream_index
            && stream_index != rmd->audio.stream_index) {
            AVStream* stream = format_ctx->streams[stream_index];
            stream->discard = AVDISCARD_ALL;
        }
    }

    if (rmd->video.stream_index == INVALID_STREAM
        && rmd->audio.stream_index == INVALID_STREAM) {
        av_log(NULL, AV_LOG_FATAL, "%s: could not find audio or video stream\n", filename);
        goto error;
    }

    if ((r = initial_seek(rmd, config->start_frame, filename)) < 0)
        goto error;

    if ((r = init_decoder_info(rmd, config)) < 0)
        goto error;

    return rmd;

error:
    rawmedia_destroy_decoder(rmd);
    return NULL;
}

void rawmedia_destroy_decoder(RawMediaDecoder* rmd) {
    if (rmd) {
        if (rmd->format_ctx) {
            if (rmd->video.stream_index != INVALID_STREAM) {
                avfilter_unref_buffer(rmd->video.picref);
                avfilter_graph_free(&rmd->video.filter_graph);
                avcodec_close(get_avstream(rmd, rmd->video.stream_index)->codec);
                packet_queue_flush(&rmd->video.packetq);
                av_free(rmd->video.avframe);
            }
            if (rmd->audio.stream_index != INVALID_STREAM) {
                avfilter_unref_buffer(rmd->audio.samplesref);
                avfilter_graph_free(&rmd->audio.filter_graph);
                avcodec_close(get_avstream(rmd, rmd->audio.stream_index)->codec);
                packet_queue_flush(&rmd->audio.packetq);
                av_free(rmd->audio.avframe);
                // Don't free audio.pkt_partial, it's a copy of audio.pkt
                av_free_packet(&rmd->audio.pkt);
            }

            avformat_close_input(&rmd->format_ctx);
        }
        av_free(rmd);
    }
}

const RawMediaDecoderInfo* rawmedia_get_decoder_info(const RawMediaDecoder* rmd) {
    return &rmd->info;
}

// Read a packet from the indicated stream.
// Return 0 on success, <0 on error.
static int read_packet(RawMediaDecoder* rmd, int stream_index, AVPacket* pkt) {
    int r = 0;
    if (stream_index == rmd->video.stream_index) {
        if (packet_queue_get(&rmd->video.packetq, pkt))
            return 0;
        if (rmd->video.status != SS_NORMAL) {
            if (rmd->video.status == SS_EOF_PENDING)
                rmd->video.status = SS_EOF;
            goto empty_packet;
        }
    }
    else if (stream_index == rmd->audio.stream_index) {
        if (packet_queue_get(&rmd->audio.packetq, pkt))
            return 0;
        if (rmd->audio.status != SS_NORMAL) {
            if (rmd->audio.status == SS_EOF_PENDING)
                rmd->audio.status = SS_EOF;
            goto empty_packet;
        }
    }

    // No queued packets. Read until we get one for our stream,
    // queuing any packets for the other stream.
    while ((r = av_read_frame(rmd->format_ctx, pkt)) >= 0) {
        if (stream_index == pkt->stream_index)
            return 0;
        else if (rmd->video.stream_index == pkt->stream_index) {
            if ((r = packet_queue_put(&rmd->video.packetq, pkt)) < 0)
                return r;
        }
        else if (rmd->audio.stream_index == pkt->stream_index)
            if ((r = packet_queue_put(&rmd->audio.packetq, pkt)) < 0)
                return r;
    }

    if (r == AVERROR_EOF) {
        if (stream_index == rmd->video.stream_index) {
            AVCodec* codec = get_avstream(rmd, rmd->video.stream_index)->codec->codec;
            if (codec->capabilities & CODEC_CAP_DELAY)
                rmd->video.status = SS_EOF_PENDING;
            else
                rmd->video.status = SS_EOF;
            goto empty_packet;
        }
        else if (stream_index == rmd->audio.stream_index) {
            AVCodec* codec = get_avstream(rmd, rmd->audio.stream_index)->codec->codec;
            if (codec->capabilities & CODEC_CAP_DELAY)
                rmd->audio.status = SS_EOF_PENDING;
            else
                rmd->audio.status = SS_EOF;
            goto empty_packet;
        }
    }
    return r;

empty_packet:
    memset(pkt, 0, sizeof(*pkt));
    av_init_packet(pkt);
    return 0;
}

static int64_t video_expected_pts(RawMediaDecoder* rmd) {
    struct RawMediaVideo* video = &rmd->video;
    AVStream* stream = get_avstream(rmd, video->stream_index);
    int64_t expected_pts = video->current_frame * video->frame_duration;
    if (stream->start_time != AV_NOPTS_VALUE)
        expected_pts += stream->start_time;
    return expected_pts;
}

static inline int64_t video_frame_pts(AVFrame* avframe) {
    return *(int64_t*)av_opt_ptr(avcodec_get_frame_class(), avframe,
                                 "best_effort_timestamp");
}

// Returns 0 if no new frame decoded (EOF), >0 if new frame decoded, <0 on error.
static int decode_video_frame(RawMediaDecoder* rmd) {
    int r = 0;
    AVCodecContext *video_ctx = get_avstream(rmd, rmd->video.stream_index)->codec;
    AVPacket pkt;
    int got_picture = 0;
    while (!got_picture && (r = read_packet(rmd, rmd->video.stream_index, &pkt)) >= 0) {
        avcodec_get_frame_defaults(rmd->video.avframe);
        if ((r = avcodec_decode_video2(video_ctx, rmd->video.avframe, &got_picture, &pkt)) < 0)
            return r;
        av_free_packet(&pkt);
        if (rmd->video.status == SS_EOF)
            return got_picture;
    }
    return r < 0 ? r : got_picture;
}

// Filters decoded video from avframe to picref
static int filter_video(RawMediaDecoder* rmd) {
    int r = 0;
    struct RawMediaVideo* video = &rmd->video;
    if ((r = av_vsrc_buffer_add_frame(video->buffersrc_ctx, video->avframe, 0)) < 0)
        return r;
    if (avfilter_poll_frame(video->buffersink_ctx->inputs[0])) {
        // Unref previous buffer
        avfilter_unref_bufferp(&video->picref);
        if ((r = av_buffersink_get_buffer_ref(video->buffersink_ctx,
                                              &video->picref, 0)) < 0)
            return r;
    }
    return r;
}

// Returns 0 if no new frame decoded, >0 if new frame decoded, <0 on error.
static int next_video_frame(RawMediaDecoder* rmd, int64_t expected_pts) {
    int r = 0;
    while (expected_pts > video_frame_pts(rmd->video.avframe)) {
        if ((r = decode_video_frame(rmd)) < 0)
            return r;
        if (rmd->video.status == SS_EOF)
            return r;
    }
    return r;
}

// Return <0 on error.
// Returns >0 if frame decoded.
// Returns 0 if no new frame decoded (EOF)
// output will be set to point to internal memory valid until the next call
// linesize will be set to the line stride size of output.
//   Multiply by height to get total buffer size.
int rawmedia_decode_video(RawMediaDecoder* rmd, uint8_t** output, int* linesize) {
    int r = 0;
    struct RawMediaVideo* video = &rmd->video;
    *linesize = 0;
    *output = NULL;

    if (video->status == SS_EOF) {
        r = 0;
        goto done;
    }

    int64_t expected_pts = video_expected_pts(rmd);
    if ((r = next_video_frame(rmd, expected_pts)) < 0)
        return r;

    if (video->status == SS_EOF) {
        r = 0;
        goto done;
    }

    // If we decoded a new frame, filter it
    if (video->avframe->format != PIX_FMT_NONE) {
        if ((r = filter_video(rmd)) < 0)
            return r;
        video->current_frame++;
        r = 1;
    }
    else
        r = 0;

done:
    if (video->picref) {
        *linesize = video->picref->linesize[0];
        *output = video->picref->data[0];
    }
    return r;
}

// Decode partial frame.
// Return <0 on error, 0 if no frame decoded, >0 if frame decoded
static int decode_partial_audio_frame(RawMediaDecoder* rmd) {
    int r = 0;
    AVCodecContext *audio_ctx = get_avstream(rmd, rmd->audio.stream_index)->codec;
    AVPacket* pkt = &rmd->audio.pkt;
    AVPacket* pkt_partial = &rmd->audio.pkt_partial;
    int got_frame = 0;
    while (pkt_partial->size > 0) {
        avcodec_get_frame_defaults(rmd->audio.avframe);
        if ((r = avcodec_decode_audio4(audio_ctx, rmd->audio.avframe, &got_frame, pkt_partial)) < 0)
            return r;
        pkt_partial->data += r;
        pkt_partial->size -= r;
        if (got_frame)
            return 1;
    }

    // Partial packet is now empty, reset
    memset(pkt_partial, 0, sizeof(*pkt_partial));
    av_free_packet(pkt);
    return 0;
}

// Decode a full audio frame.
// Return <0 on error, >0 when frame decoded.
static int decode_audio_frame(RawMediaDecoder* rmd) {
    int r = 0;
    AVPacket* pkt = &rmd->audio.pkt;
    AVPacket* pkt_partial = &rmd->audio.pkt_partial;
    do {
        // Read anything left in partial, return on error or got frame
        if ((r = decode_partial_audio_frame(rmd)))
            return r;
        // If partial exausted and no frame, read a new packet and try again
        if ((r = read_packet(rmd, rmd->audio.stream_index, pkt)) >= 0)
            *pkt_partial = *pkt;
        if (rmd->audio.status == SS_EOF)
            return r;
    } while (r >= 0);
    return r;
}

// Filters decoded audio data from avframe to samplesref
static int filter_audio(RawMediaDecoder* rmd) {
    int r = 0;
    struct RawMediaAudio* audio = &rmd->audio;
    AVFrame* avframe = audio->avframe;
    if ((r = av_asrc_buffer_add_samples(audio->abuffersrc_ctx,
                                        avframe->data, avframe->linesize,
                                        avframe->nb_samples,
                                        avframe->sample_rate, avframe->format,
                                        avframe->channel_layout,
                                        av_sample_fmt_is_planar(avframe->format),
                                        avframe->pts, 0)) < 0)
        return r;
    if (avfilter_poll_frame(audio->abuffersink_ctx->inputs[0])) {
        // Unref previous buffer
        avfilter_unref_bufferp(&audio->samplesref);
        if ((r = av_buffersink_get_buffer_ref(audio->abuffersink_ctx,
                                              &audio->samplesref, 0)) < 0)
            return r;
        audio->nb_samples_consumed = 0;
    }
    return r;
}

// Discard all input audio samples up until start_sample.
// start_sample is in source audio timebase
static int first_audio_frame(RawMediaDecoder* rmd, int64_t start_sample) {
    int r = 0;
    AVRational audio_time_base = get_avstream(rmd, rmd->audio.stream_index)->time_base;
    int64_t samples_per_frame = av_rescale_q(1, rmd->time_base, audio_time_base);
    int64_t end_sample = start_sample + samples_per_frame - 1;
    int64_t decoded_start_sample = 0;
    int64_t decoded_end_sample = 0;
    for (;;) {
        if (decoded_start_sample <= end_sample
            && decoded_end_sample >= start_sample)
            break;
        if ((r = decode_audio_frame(rmd)) < 0)
            return r;
        decoded_start_sample += rmd->audio.avframe->nb_samples;
        decoded_end_sample = decoded_start_sample + samples_per_frame - 1;
        if (rmd->audio.status == SS_EOF)
            return r;
    }

    int discard_input_nb_samples = decoded_start_sample - start_sample;
    if (discard_input_nb_samples <= 0)
        return r;

    int discard_output_nb_samples =
        av_rescale_q(discard_input_nb_samples, audio_time_base,
                     RAWMEDIA_AUDIO_TIME_BASE);

    // Filter audio then set samples consumed so we skip the
    // samples being discarded
    if ((r = filter_audio(rmd)) < 0)
        return r;
    rmd->audio.nb_samples_consumed = discard_output_nb_samples;

    return r;
}

// Copies as much audio from samplesref into output as will fit.
// Updates output and output_nb_samples.
// Destroys samplesref if fully consumed.
static void copy_audio(RawMediaDecoder* rmd, uint8_t** output, int* output_nb_samples) {
    struct RawMediaAudio* audio = &rmd->audio;
    if (!audio->samplesref)
        return;
    int nb_samples = FFMIN(*output_nb_samples,
                           audio->samplesref->audio->nb_samples
                           - audio->nb_samples_consumed);
    int bytes_per_sample = av_get_bytes_per_sample(RAWMEDIA_AUDIO_SAMPLE_FMT)
        * av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
    int nb_bytes = nb_samples * bytes_per_sample;
    uint8_t* input = audio->samplesref->data[0]
        + (audio->nb_samples_consumed * bytes_per_sample);

    memcpy(*output, input, nb_bytes);

    *output_nb_samples -= nb_samples;
    *output += nb_bytes;
    audio->nb_samples_consumed += nb_samples;

    // Fully consumed samplesref
    if (audio->nb_samples_consumed >= audio->samplesref->audio->nb_samples) {
        avfilter_unref_bufferp(&audio->samplesref);
        audio->nb_samples_consumed = 0;
    }
}

// Return <0 on error.
// Decodes silent output after EOF.
int rawmedia_decode_audio(RawMediaDecoder* rmd, uint8_t* output) {
    int r = 0;
    struct RawMediaAudio* audio = &rmd->audio;
    int output_nb_samples = audio->output_samples_per_frame;

    // Copy any remaining samples in samplesref
    if (audio->samplesref)
        copy_audio(rmd, &output, &output_nb_samples);

    if (rmd->audio.status != SS_EOF) {
        // Decode, filter and copy until output full, or nothing to decode (EOF)
        while (output_nb_samples > 0 && (r = decode_audio_frame(rmd)) > 0) {
            if ((r = filter_audio(rmd)) < 0)
                return r;
            copy_audio(rmd, &output, &output_nb_samples);
        }
    }

    // Pad output with silence
    if (output_nb_samples > 0) {
        int bytes = output_nb_samples
            * av_get_bytes_per_sample(RAWMEDIA_AUDIO_SAMPLE_FMT)
            * av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
        memset(output, RAWMEDIA_AUDIO_SILENCE, bytes);
        return r;
    }

    return r;
}
