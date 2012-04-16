// gcc -std=c99 -ggdb3 $(/opt/motionbox/foundation/5.4.8/bin/pkg-config --cflags --libs libavcodec libavformat libavcore libavutil libswscale) -o decoder decoder.c

// See http://web.me.com/dhoerl/Home/Tech_Blog/Entries/2009/1/22_Revised_avcodec_sample.c.html
// Also see shotdetect code

//XXX also need lossless encoder class - see libavformat/output-example.c

#include "rawmedia_internal.h"
#include "decoder.h"
#include "packet_queue.h"


#define INVALID_STREAM -1

//XXX split these into nested audio/video struct and rename members
struct RawMediaDecoder {
    AVFormatContext* format_ctx;
    AVRational timebase;

    int video_stream;
    PacketQueue videoq;
    AVFrame* video_frame;
    uint32_t video_next_frame;
    uint32_t video_frame_duration;    // Frame duration in video timebase
    uint32_t video_width;
    uint32_t video_height;

    int audio_stream;
    PacketQueue audioq;
    AVFrame* audio_frame;
    uint32_t audio_next_frame;
    uint32_t audio_frame_duration;    // Frame duration in audio timebase
    AVPacket audio_pkt;
    AVPacket audio_pkt_partial;
};

static int open_decoder(RawMediaDecoder* rmd, int stream) {
    int r = 0;
    if (stream == INVALID_STREAM)
        return r;
    AVCodecContext *ctx = rmd->format_ctx->streams[stream]->codec;
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

RawMediaDecoder* rawmedia_create_decoder(const char* filename, const RawMediaDecoderConfig* config) {
    int r = 0;
    RawMediaDecoder* rmd = av_mallocz(sizeof(RawMediaDecoder));
    AVFormatContext* format_ctx = NULL;
    if (!rmd)
        return NULL;

    rmd->video_stream = INVALID_STREAM;
    rmd->audio_stream = INVALID_STREAM;

    rmd->timebase = (AVRational){config->framerate.den, config->framerate.num};

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

    for (int j = 0; j < format_ctx->nb_streams; j++) {
        AVStream* stream = format_ctx->streams[j];
        switch (stream->codec->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            if (!config->discard_video && rmd->video_stream == INVALID_STREAM) {
                rmd->video_stream = j;
                if ((r = open_decoder(rmd, rmd->video_stream)) < 0) {
                    av_log(NULL, AV_LOG_FATAL,
                           "%s: failed to open video decoder\n", filename);
                    goto error;
                }
                if (!(rmd->video_frame = avcodec_alloc_frame()))
                    goto error;
                rmd->video_frame_duration = av_rescale_q(1, rmd->timebase, stream->time_base);
                rmd->video_next_frame = config->start_frame;

                //XXX if we add support for config bounding box, take it into account here
                rmd->video_height = stream->codec->height;
                if (stream->sample_aspect_ratio.num) {
                    float aspect_ratio = av_q2d(stream->sample_aspect_ratio);
                    rmd->video_width = (uint32_t)rint(rmd->video_height * aspect_ratio);
                }
                else
                    rmd->video_width = stream->codec->width;
            }
            else
                format_ctx->streams[j]->discard = AVDISCARD_ALL;
            break;
        case AVMEDIA_TYPE_AUDIO:
            if (!config->discard_audio && rmd->audio_stream == INVALID_STREAM){
                rmd->audio_stream = j;
                if ((r = open_decoder(rmd, rmd->audio_stream)) < 0) {
                    av_log(NULL, AV_LOG_FATAL,
                           "%s: failed to open audio decoder\n", filename);
                    goto error;
                }
                if (!(rmd->audio_frame = avcodec_alloc_frame()))
                    goto error;
                rmd->audio_frame_duration = av_rescale_q(1, rmd->timebase, stream->time_base);
                rmd->audio_next_frame = config->start_frame;
            }
            else
                format_ctx->streams[j]->discard = AVDISCARD_ALL;
            break;
        default:
            format_ctx->streams[j]->discard = AVDISCARD_ALL;
            break;
        }
    }
    if (rmd->video_stream == INVALID_STREAM && rmd->audio_stream == INVALID_STREAM) {
        av_log(NULL, AV_LOG_FATAL, "%s: could not find audio or video stream\n", filename);
        goto error;
    }

    if (config->start_frame != 0) {
        int64_t timestamp = av_rescale_q(config->start_frame, rmd->timebase, AV_TIME_BASE_Q);
        if (av_seek_frame(format_ctx, -1, timestamp, AVSEEK_FLAG_BACKWARD) < 0)
            av_log(NULL, AV_LOG_WARNING, "%s: failed to seek\n", filename);
    }

    return rmd;

error:
    //XXX log result code - need logging hook integration (for av logging too)
    rawmedia_destroy_decoder(rmd);
    return NULL;
}

void rawmedia_destroy_decoder(RawMediaDecoder* rmd) {
    if (rmd) {
        if (rmd->format_ctx) {
            if (rmd->video_stream != INVALID_STREAM) {
                avcodec_close(rmd->format_ctx->streams[rmd->video_stream]->codec);
                packet_queue_flush(&rmd->videoq);
                av_free(rmd->video_frame);
            }
            if (rmd->audio_stream != INVALID_STREAM) {
                avcodec_close(rmd->format_ctx->streams[rmd->audio_stream]->codec);
                packet_queue_flush(&rmd->audioq);
                av_free(rmd->audio_frame);
                av_free_packet(&rmd->audio_pkt);
                // Don't free audio_pkt_partial, it's a copy of audio_pkt
            }

            avformat_close_input(&rmd->format_ctx);
        }
        av_free(rmd);
    }
}

static int64_t stream_duration_frames(const RawMediaDecoder* rmd, int stream, uint32_t stream_frame_duration, uint32_t start_frame) {
    if (stream != INVALID_STREAM) {
        int64_t duration = rmd->format_ctx->streams[stream]->duration;
        int64_t frames = duration / stream_frame_duration;
        // Round up if partial frame
        if (duration % stream_frame_duration)
            frames++;
        frames -= start_frame;
        return frames;
    }
    else
        return 0;
}

int rawmedia_get_decoder_info(const RawMediaDecoder* rmd, RawMediaDecoderInfo* info) {
    *info = (RawMediaDecoderInfo){0};

    if (rmd->video_stream != INVALID_STREAM) {
        info->has_video = 1;
        int64_t duration = stream_duration_frames(rmd, rmd->video_stream,
                                                  rmd->video_frame_duration,
                                                  rmd->video_next_frame);
        if (info->duration < duration)
            info->duration = duration;
        info->video_width = rmd->video_width;
        info->video_height = rmd->video_height;
#if RAWMEDIA_VIDEO_PIXEL_FORMAT == PIX_FMT_RGB24
        info->video_framebuffer_size = info->video_width * info->video_height * 3;
#else
#error Recompute framebuffer size for new pixel format
#endif
    }

    if (rmd->audio_stream != INVALID_STREAM) {
        info->has_audio = 1;
        int64_t duration = stream_duration_frames(rmd, rmd->audio_stream,
                                                  rmd->audio_frame_duration,
                                                  rmd->audio_next_frame);
        if (info->duration < duration)
            info->duration = duration;

        // How many samples per frame at our target framerate
        //XXX probably need this in rmd for when we are decoding
        int64_t samples = av_rescale_q(1,
                                       rmd->timebase,
                                       (AVRational){1, RAWMEDIA_AUDIO_SAMPLE_RATE});

        int size = av_samples_get_buffer_size(NULL, RAWMEDIA_AUDIO_CHANNELS,
                                              (int)samples,
                                              RAWMEDIA_AUDIO_SAMPLE_FMT, 1);
        if (size <= 0)
            return -1;
        info->audio_framebuffer_size = (uint32_t)size;
    }
    return 0;
}

// Read a packet from the indicated stream.
// Return 0 on success, <0 on error or AVERROR_EOF
static int read_packet(RawMediaDecoder* rmd, int stream, AVPacket* pkt) {
    int r = 0;
    if (stream == rmd->video_stream) {
        if (packet_queue_get(&rmd->videoq, pkt))
            return 0;
    }
    else if (stream == rmd->audio_stream) {
        if (packet_queue_get(&rmd->audioq, pkt))
            return 0;
    }

    // No queued packats. Read until we get one for our stream,
    // queuing any packets for the other stream.
    while ((r = av_read_frame(rmd->format_ctx, pkt)) >= 0) {
        if (stream == pkt->stream_index)
            return 0;
        else if (rmd->video_stream == pkt->stream_index) {
            if ((r = packet_queue_put(&rmd->videoq, pkt)) < 0)
                return r;
        }
        else if (rmd->audio_stream == pkt->stream_index)
            if ((r = packet_queue_put(&rmd->audioq, pkt)) < 0)
                return r;
    }
    //XXX on EOF, should send pkt with null data to decoder if CODEC_CAP_DELAY
    //XXX need more explicit EOF handling - track in context
    return r;
}

static int decode_video_frame(RawMediaDecoder* rmd) {
    int r = 0;
    AVCodecContext *video_ctx = rmd->format_ctx->streams[rmd->video_stream]->codec;
    AVPacket pkt;
    int got_picture = 0;
    while (!got_picture && (r = read_packet(rmd, rmd->video_stream, &pkt)) >= 0) {
        avcodec_get_frame_defaults(rmd->video_frame);
        if ((r = avcodec_decode_video2(video_ctx, rmd->video_frame, &got_picture, &pkt)) < 0)
            return r;
        av_free_packet(&pkt);
    }
    return r;
}

//XXX for audio/video, continue returning frames after EOF (silence for audio, last frame for video) - caller knows duration and should stop when they want

// Return <0 on error or AVERROR_EOF
int rawmedia_decode_video(RawMediaDecoder* rmd) {
    int r = 0;
    int64_t expected_pts = rmd->video_next_frame * rmd->video_frame_duration;

    //XXX check video_frame pts and expected, swscale current frame into output if OK

    if ((r = decode_video_frame(rmd)) < 0)
        return r;

    //XXX check video_frame pts, if < expected, loop decoding video frames until we get one we can use - then swscale it into output

    rmd->video_next_frame++;
    return r;
}

// Decode partial frame.
// Return <0 on error, 0 if no frame decoded, >0 if frame decoded
static int decode_partial_audio_frame(RawMediaDecoder* rmd) {
    int r = 0;
    AVCodecContext *audio_ctx = rmd->format_ctx->streams[rmd->audio_stream]->codec;
    AVPacket* pkt = &rmd->audio_pkt;
    AVPacket* pkt_partial = &rmd->audio_pkt_partial;
    int got_frame = 0;
    while (pkt_partial->size > 0) {
        avcodec_get_frame_defaults(rmd->audio_frame);
        if ((r = avcodec_decode_audio4(audio_ctx, rmd->audio_frame, &got_frame, pkt_partial)) < 0)
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
    AVPacket* pkt = &rmd->audio_pkt;
    AVPacket* pkt_partial = &rmd->audio_pkt_partial;
    do {
        // Read anything left in partial, return on error or got frame
        if ((r = decode_partial_audio_frame(rmd)))
            return r;
        // If partial exausted and no frame, read a new packet and try again
        if ((r = read_packet(rmd, rmd->audio_stream, pkt)) >= 0)
            *pkt_partial = *pkt;
    } while (r >= 0);
    return r;
}

int rawmedia_decode_audio(RawMediaDecoder* rmd) {
    int r = 0;
    int64_t expected_pts = rmd->audio_next_frame * rmd->audio_frame_duration;

    //XXX need to keep track of offset in audio_frame and resample any partial frame to output

    if ((r = decode_audio_frame(rmd)) < 0)
        return r;

    //XXX now need to resample from audio_frame to output, and track how much we consumed - and decode another frame if we don't fill output buffer

    rmd->audio_next_frame++;
    return r;
}
