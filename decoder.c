#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
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
        int width;
        int height;
        struct SwsContext* sws_ctx;
        enum StreamStatus status;
    } video;

    struct RawMediaAudio {
        int stream_index;
        PacketQueue packetq;
        AVFrame* avframe;
        int output_samples_per_frame;
        AVPacket pkt;
        AVPacket pkt_partial;
        struct SwrContext* swr_ctx;
        enum StreamStatus status;
    } audio;

    RawMediaDecoderInfo info;
};

static inline int64_t frame_pts(AVFrame* avframe);
static int next_video_frame(RawMediaDecoder* rmd, int64_t expected_pts);
static int first_audio_frame(RawMediaDecoder* rmd, int64_t start_sample);

static inline AVStream* get_avstream(const RawMediaDecoder* rmd, int stream_index) {
    return rmd->format_ctx->streams[stream_index];
}

static int open_decoder(RawMediaDecoder* rmd, int stream_index) {
    int r = 0;
    if (stream_index == INVALID_STREAM)
        return r;
    AVCodecContext *ctx = get_avstream(rmd, stream_index)->codec;
    AVCodec *codec = avcodec_find_decoder(ctx->codec_id);
    if (codec == NULL)
        return -1;
    // Disable threading, introduces codec delay
    AVDictionary* opts = NULL;
    av_dict_set(&opts, "threads", "1", 0);
    r = avcodec_open2(ctx, codec, &opts);
    av_dict_free(&opts);
    return r;
}

static int create_audio_resample_ctx(RawMediaDecoder* rmd) {
    struct RawMediaAudio* audio = &rmd->audio;
    AVCodecContext *audio_ctx = get_avstream(rmd, audio->stream_index)->codec;
    int64_t channel_layout = (audio_ctx->channel_layout
                              && audio_ctx->channels == av_get_channel_layout_nb_channels(audio_ctx->channel_layout))
        ? audio_ctx->channel_layout
        : av_get_default_channel_layout(audio_ctx->channels);

    audio->swr_ctx =
        swr_alloc_set_opts(NULL,
                           RAWMEDIA_AUDIO_CHANNEL_LAYOUT,
                           RAWMEDIA_AUDIO_SAMPLE_FMT,
                           RAWMEDIA_AUDIO_SAMPLE_RATE,
                           channel_layout,
                           audio_ctx->sample_fmt,
                           audio_ctx->sample_rate,
                           0, NULL);
    if (!audio->swr_ctx || swr_init(audio->swr_ctx) < 0)
        return -1;
    return 0;
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
        video_pts = rmd->video.current_frame * rmd->video.frame_duration;
        if ((r = next_video_frame(rmd, video_pts)) < 0) {
            av_log(NULL, AV_LOG_FATAL, "%s: failed to seek video\n", filename);
            return r;
        }
        // Save actual pts of frame we used, to compute audio offset below
        video_pts = frame_pts(rmd->video.avframe);
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
        info->width = rmd->video.width;
        info->height = rmd->video.height;
        int size = avpicture_get_size(RAWMEDIA_VIDEO_DECODE_PIXEL_FORMAT,
                                      info->width, info->height);
        if (size <= 0)
            return -1;
        info->video_framebuffer_size = size;
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
    int r = 0;
    RawMediaDecoder* rmd = av_mallocz(sizeof(RawMediaDecoder));
    AVFormatContext* format_ctx = NULL;
    if (!rmd)
        return NULL;

    rmd->video.stream_index = INVALID_STREAM;
    rmd->audio.stream_index = INVALID_STREAM;

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

    for (int stream_index = 0; stream_index < format_ctx->nb_streams; stream_index++) {
        AVStream* stream = format_ctx->streams[stream_index];
        switch (stream->codec->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            if (!config->discard_video && rmd->video.stream_index == INVALID_STREAM) {
                rmd->video.stream_index = stream_index;
                if ((r = open_decoder(rmd, rmd->video.stream_index)) < 0) {
                    av_log(NULL, AV_LOG_FATAL,
                           "%s: failed to open video decoder\n", filename);
                    goto error;
                }
                if (!(rmd->video.avframe = avcodec_alloc_frame()))
                    goto error;
                rmd->video.frame_duration = av_rescale_q(1, rmd->time_base,
                                                         stream->time_base);
                rmd->video.current_frame = config->start_frame;

                //XXX if we add support for config bounding box, take it into account here
                rmd->video.height = stream->codec->height;
                if (stream->sample_aspect_ratio.num) {
                    float aspect_ratio = av_q2d(stream->sample_aspect_ratio);
                    rmd->video.width = rint(rmd->video.height * aspect_ratio);
                }
                else
                    rmd->video.width = stream->codec->width;
            }
            else
                stream->discard = AVDISCARD_ALL;
            break;

        case AVMEDIA_TYPE_AUDIO:
            if (!config->discard_audio && rmd->audio.stream_index == INVALID_STREAM){
                rmd->audio.stream_index = stream_index;
                if ((r = open_decoder(rmd, rmd->audio.stream_index)) < 0) {
                    av_log(NULL, AV_LOG_FATAL,
                           "%s: failed to open audio decoder\n", filename);
                    goto error;
                }
                if (!(rmd->audio.avframe = avcodec_alloc_frame()))
                    goto error;
                // How many samples per frame at our target framerate/samplerate
                rmd->audio.output_samples_per_frame =
                    av_rescale_q(1, rmd->time_base, RAWMEDIA_AUDIO_TIME_BASE);

                if (create_audio_resample_ctx(rmd) < 0) {
                    av_log(NULL, AV_LOG_ERROR,
                           "%s: failed to create audio resampling context\n", filename);
                    goto error;
                }
            }
            else
                stream->discard = AVDISCARD_ALL;
            break;
        default:
            stream->discard = AVDISCARD_ALL;
            break;
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
                avcodec_close(get_avstream(rmd, rmd->video.stream_index)->codec);
                packet_queue_flush(&rmd->video.packetq);
                av_free(rmd->video.avframe);
                sws_freeContext(rmd->video.sws_ctx);
            }
            if (rmd->audio.stream_index != INVALID_STREAM) {
                avcodec_close(get_avstream(rmd, rmd->audio.stream_index)->codec);
                packet_queue_flush(&rmd->audio.packetq);
                av_free(rmd->audio.avframe);
                // Don't free audio.pkt_partial, it's a copy of audio.pkt
                av_free_packet(&rmd->audio.pkt);
                swr_free(&rmd->audio.swr_ctx);
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

static inline int64_t frame_pts(AVFrame* avframe) {
    return *(int64_t*)av_opt_ptr(avcodec_get_frame_class(), avframe,
                                 "best_effort_timestamp");
}

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
            return r;
    }
    return r;
}

static int scale_video(RawMediaDecoder* rmd, uint8_t* output) {
    struct RawMediaVideo* video = &rmd->video;
    AVFrame* frame = video->avframe;
    rmd->video.sws_ctx =
        sws_getCachedContext(video->sws_ctx,
                             frame->width, frame->height,
                             frame->format,
                             video->width, video->height,
                             RAWMEDIA_VIDEO_DECODE_PIXEL_FORMAT,
                             SWS_LANCZOS|SWS_ACCURATE_RND|SWS_FULL_CHR_H_INT,
                             NULL, NULL, NULL);
    if (!video->sws_ctx)
        return -1;
    AVPicture picture;
    avpicture_fill(&picture, output, RAWMEDIA_VIDEO_DECODE_PIXEL_FORMAT,
                   video->width, video->height);
    sws_scale(video->sws_ctx,
              (const uint8_t* const*)frame->data, frame->linesize,
              0, frame->height,
              picture.data, picture.linesize);
    return 0;
}

static int next_video_frame(RawMediaDecoder* rmd, int64_t expected_pts) {
    int r = 0;
    while (expected_pts > frame_pts(rmd->video.avframe)) {
        if ((r = decode_video_frame(rmd)) < 0)
            return r;
        if (rmd->video.status == SS_EOF)
            return r;
    }
    return r;
}

// Return <0 on error.
// Returns >0 if frame decoded.
// Returns 0 if no frame decoded (EOF) and output was not modified.
int rawmedia_decode_video(RawMediaDecoder* rmd, uint8_t* output) {
    int r = 0;

    if (rmd->video.status == SS_EOF)
        return 0;

    int64_t expected_pts = rmd->video.current_frame * rmd->video.frame_duration;
    if ((r = next_video_frame(rmd, expected_pts)) < 0)
        return r;

    if (rmd->video.status == SS_EOF)
        return 0;

    if ((r = scale_video(rmd, output)) < 0)
        return r;

    rmd->video.current_frame++;
    return 1;
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

// Discard all input audio samples up until start_sample.
// start_sample and samples_per_frame are both in source audio timebase
static int first_audio_frame(RawMediaDecoder* rmd, int64_t start_sample) {
    int r = 0;
    AVRational audio_time_base = get_avstream(rmd, rmd->audio.stream_index)->time_base;
    int64_t samples_per_frame = av_rescale_q(1, rmd->time_base, audio_time_base);
    int64_t end_sample = start_sample + samples_per_frame - 1;
    int decoded_nb_samples = 0;
    for (;;) {
        if (start_sample <= decoded_nb_samples && decoded_nb_samples <= end_sample)
            break;
        if ((r = decode_audio_frame(rmd)) < 0)
            return r;
        decoded_nb_samples += rmd->audio.avframe->nb_samples;
        if (rmd->audio.status == SS_EOF)
            return r;
    }

    int discard_input_nb_samples = decoded_nb_samples - start_sample;
    if (discard_input_nb_samples <= 0)
        return r;

    int discard_output_nb_samples =
        av_rescale_q(discard_input_nb_samples, audio_time_base,
                     RAWMEDIA_AUDIO_TIME_BASE);

    int nb_channels = av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);
    int bufsize = av_samples_get_buffer_size(NULL, nb_channels,
                                             discard_output_nb_samples,
                                             RAWMEDIA_AUDIO_SAMPLE_FMT, 1);
    uint8_t buf[bufsize];
    uint8_t *output[] = { buf };

    const uint8_t** input = (const uint8_t**)rmd->audio.avframe->data;
    int input_nb_samples = rmd->audio.avframe->nb_samples;

    // Resampler will buffer unused input samples if needed.
    while ((r = swr_convert(rmd->audio.swr_ctx,
                            output, discard_output_nb_samples,
                            input, input_nb_samples)) > 0) {
        input = NULL;
        input_nb_samples = 0;
        discard_output_nb_samples -= r;
    }

    return r;
}

// Resample input, can be NULL/0 to drain resampler buffer.
// output/output_nb_samples will be updated.
static int resample_audio(RawMediaDecoder* rmd, uint8_t **output, int* output_nb_samples, const uint8_t **input, int input_nb_samples) {
    int r = 0;
    int bytes_per_sample = av_get_bytes_per_sample(RAWMEDIA_AUDIO_SAMPLE_FMT)
        * av_get_channel_layout_nb_channels(RAWMEDIA_AUDIO_CHANNEL_LAYOUT);

    uint8_t *outbuf[] = { *output };

    while (*output_nb_samples > 0
           && (r = swr_convert(rmd->audio.swr_ctx, outbuf, *output_nb_samples,
                               input, input_nb_samples)) > 0) {
        input = NULL;
        input_nb_samples = 0;
        *output += r * bytes_per_sample;
        outbuf[0] = *output;
        *output_nb_samples -= r;
    }
    return r;
}

// Return <0 on error.
// Decodes silent output after EOF.
int rawmedia_decode_audio(RawMediaDecoder* rmd, uint8_t* output) {
    int r = 0;
    int output_nb_samples = rmd->audio.output_samples_per_frame;

    // Drain resampler buffer
    if ((r = resample_audio(rmd, &output, &output_nb_samples, NULL, 0)) < 0)
        return r;

    if (rmd->audio.status != SS_EOF) {
        while (output_nb_samples > 0 && (r = decode_audio_frame(rmd)) > 0) {
            const uint8_t** input = (const uint8_t**)rmd->audio.avframe->data;
            int input_nb_samples = rmd->audio.avframe->nb_samples;

            if ((r = resample_audio(rmd, &output, &output_nb_samples,
                                    input, input_nb_samples)) < 0)
                return r;
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
