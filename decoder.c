// gcc -std=c99 -ggdb3 $(/opt/motionbox/foundation/5.4.8/bin/pkg-config --cflags --libs libavcodec libavformat libavcore libavutil libswscale) -o decoder decoder.c

// See http://web.me.com/dhoerl/Home/Tech_Blog/Entries/2009/1/22_Revised_avcodec_sample.c.html
// Also see shotdetect code

//XXX also need lossless encoder class - see libavformat/output-example.c

#include "transcoder.h"
#include "packet_queue.h"
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>



static int const INVALID_STREAM = -1;

struct DecoderContext {
    AVFormatContext* format_ctx;

    int video_stream;
    PacketQueue videoq;
    AVFrame* video_frame;

    int audio_stream;
    PacketQueue audioq;
    AVFrame* audio_frame;
};

static int open_decoder(DecoderContext* dc, int stream) {
    int r = 0;
    if (stream == INVALID_STREAM)
        return r;
    AVCodecContext *ctx = dc->format_ctx->streams[stream]->codec;
    AVCodec *codec = avcodec_find_decoder(ctx->codec_id);
    if (codec == NULL)
        return -1;
    if ((r = avcodec_open2(ctx, codec, NULL)) < 0)
        return r;
    return r;
}

//XXX pass in AVRational fps - so we can keep track of what next expected frame is
//XXX pass start time, need to seek and decode until that time
//XXX for end/duration, caller can keep track and shut down decoding when done
DecoderContext* create_decoder_context(const char* filename) {
    int r = 0;
    DecoderContext* dc = av_mallocz(sizeof(DecoderContext));
    AVFormatContext* format_ctx = NULL;
    if (!dc)
        return NULL;
    packet_queue_init(&dc->videoq);
    packet_queue_init(&dc->audioq);

    if ((r = avformat_open_input(&format_ctx, filename, NULL, NULL)) != 0)
        goto error;
    dc->format_ctx = format_ctx;

    if ((r = avformat_find_stream_info(format_ctx, NULL)) < 0)
        goto error;

    dc->video_stream = INVALID_STREAM;
    dc->audio_stream = INVALID_STREAM;
    for (int j = 0; j < format_ctx->nb_streams; j++) {
        switch (format_ctx->streams[j]->codec->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            if (dc->video_stream == INVALID_STREAM) {
                dc->video_stream = j;
                if ((r = open_decoder(dc, dc->video_stream)) < 0)
                    goto error;
                if (!(dc->video_frame = avcodec_alloc_frame()))
                    goto error;
            }
            break;
        case AVMEDIA_TYPE_AUDIO:
            if (dc->audio_stream == INVALID_STREAM){
                dc->audio_stream = j;
                if ((r = open_decoder(dc, dc->audio_stream)) < 0)
                    goto error;
                if (!(dc->audio_frame = avcodec_alloc_frame()))
                    goto error;
            }
            break;
        default:
            break;
        }
    }
    if (dc->video_stream == INVALID_STREAM && dc->audio_stream == INVALID_STREAM)
        goto error;

    return dc;
error:
    //XXX log result code - need logging hook integration (for av logging too)
    destroy_decoder_context(dc);
    return NULL;
}

void destroy_decoder_context(DecoderContext* dc) {
    if (dc) {
        if (dc->format_ctx) {
            if (dc->video_stream != INVALID_STREAM) {
                avcodec_close(dc->format_ctx->streams[dc->video_stream]->codec);
                packet_queue_flush(&dc->videoq);
                av_free(dc->video_frame);
            }
            if (dc->audio_stream != INVALID_STREAM) {
                avcodec_close(dc->format_ctx->streams[dc->audio_stream]->codec);
                packet_queue_flush(&dc->audioq);
                av_free(dc->audio_frame);
            }

            avformat_close_input(&dc->format_ctx);
        }
        av_free(dc);
    }
}

// Read a packet from the indicated stream.
// Return 0 on success, <0 on error or AVERROR_EOF
static int read_packet(DecoderContext* dc, int stream, AVPacket* pkt) {
    int r = 0;
    if (stream == dc->video_stream) {
        if (packet_queue_get(&dc->videoq, pkt))
            return 0;
    }
    else if (stream == dc->audio_stream) {
        if (packet_queue_get(&dc->audioq, pkt))
            return 0;
    }

    // No queued packats. Read until we get one for our stream,
    // queuing any packets for the other stream.
    while ((r = av_read_frame(dc->format_ctx, pkt)) >= 0) {
        if (stream == pkt->stream_index)
            return 0;
        else if (dc->video_stream == pkt->stream_index) {
            if ((r = packet_queue_put(&dc->videoq, pkt)) < 0)
                return r;
        }
        else if (dc->audio_stream == pkt->stream_index)
            if ((r = packet_queue_put(&dc->audioq, pkt)) < 0)
                return r;
    }
    //XXX on EOF, should send pkt with null data to decoder if CODEC_CAP_DELAY
    return r;
}

//XXX or let user pass fixed dimension buffer and swscale into RGB and letterboxed into that buffer

// Return <0 on error or AVERROR_EOF
int decode_video_frame(DecoderContext* dc) {
    int r = 0;
    AVCodecContext *video_ctx = dc->format_ctx->streams[dc->video_stream]->codec;
    AVPacket pkt;
    int got_picture = 0;
    while (got_picture == 0 && (r = read_packet(dc, dc->video_stream, &pkt)) >= 0) {
        avcodec_get_frame_defaults(dc->video_frame);
        if ((r = avcodec_decode_video2(video_ctx, dc->video_frame, &got_picture, &pkt)) < 0)
            return r;

        //XXX examine pts (best_effort_timestamp?) and keep decoding until we get the frame we need, then swscale it into callers buffer
        //XXX depending on framerate, we may need to repeat last frame (i.e. not decode a new one) - this should be OK since we save dc->video_frame so we have it (would have to rescale though)

        av_free_packet(&pkt);
    }
    //XXX if got_picture and r>=0, then copy/scale frame to output buffer
    return r;
 }

#if 0
int decode_audio_frame(DecoderContext* dc) {
    int r = 0;
    AVCodecContext *audio_ctx = dc->format_ctx->streams[dc->audio_stream]->codec;
    AVPacket pkt;
    int got_frame;
    if ((r = read_packet(dc, dc->audio_stream, &pkt)) < 0)
        return r;

    avcodec_get_frame_defaults(dc->audio_frame);
    //XXX do we set nb_samples on frame here?
    //XXX how do we specify samples to decode? and once we got_frame, what if there are samples left in the pkt - need to save it for next decode and continue with partial pkt?
    //XXX because we might be skipping frames depending what framerate we are pulling video at, so may need to decode more audio?

    //XXX do we still need pkt_temp? because pkt frees data so modifying it isn't safe below
    //XXX need to store pkt in context, and pkt_temp offsets into it - so if we fill output buffer but still have data left in pkt we can use it next time around - see avconv, has audio_pkt and audio_pkt_temp in context

    //http://git.libav.org/?p=libav.git;a=commitdiff;h=f199f38573c4c02753f03ba8db04481038fa6f2e

    while (pkt.size > 0) {
        if ((r = avcodec_decode_audio4(audio_ctx, dc->audio_frame, &got_frame, &pkt)) < 0)
            break;
        pkt.data += r;
        pkt.size -= r;
        //XXX check got_frame
    }

    av_free_packet(&pkt);
    return r;
}
#endif
