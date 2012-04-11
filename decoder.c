// On Linux:
// libgnustep-base-dev
// gcc `gnustep-config --objc-flags` main.m -lgnustep-base

// gcc -std=c99 -ggdb3 $(/opt/motionbox/foundation/5.4.8/bin/pkg-config --cflags --libs libavcodec libavformat libavcore libavutil libswscale) -o main main.m

// See http://web.me.com/dhoerl/Home/Tech_Blog/Entries/2009/1/22_Revised_avcodec_sample.c.html
// Also see shotdetect code

//XXX turn all this into an ObjC class
//XXX also need lossless encoder class - see libavformat/output-example.c

#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

int main(int argc, const char *argv[]) {
    int r = 0;

    av_register_all();
    AVFormatContext *pFormatCtx = NULL;
    if ((r = av_open_input_file(&pFormatCtx, argv[1], NULL, 0, NULL)) != 0)
        return r;

    if ((r = av_find_stream_info(pFormatCtx)) < 0)
        return r;

    int videoStream = -1;
    int audioStream = -1;
    for (int j = 0; j < pFormatCtx->nb_streams; j++) {
        switch (pFormatCtx->streams[j]->codec->codec_type) {
  	        case CODEC_TYPE_VIDEO:
  	        videoStream = j;
  	        break;
  	        case CODEC_TYPE_AUDIO:
  	        //XXX do something with audio - need ctx, decoder etc.
  	        audioStream = j;
  	        break;
  	    }
    }
    if (videoStream == -1)
        return -1;

    AVCodecContext *pVideoCtx = pFormatCtx->streams[videoStream]->codec;
    AVCodec *pVideoCodec = avcodec_find_decoder(pVideoCtx->codec_id);
    if (pVideoCodec == NULL)
        return -1;
    if ((r = avcodec_open(pVideoCtx, pVideoCodec)) < 0)
        return r;

    AVFrame *pVideoFrame = avcodec_alloc_frame();//XXXcheck
    AVFrame *pVideoFrameRGB = avcodec_alloc_frame();//XXXcheck
    uint8_t *videoBuffer = malloc(avpicture_get_size(PIX_FMT_RGB24, pVideoCtx->width, pVideoCtx->height));//XXXcheck
    avpicture_fill((AVPicture *)pVideoFrameRGB, videoBuffer, PIX_FMT_RGB24, pVideoCtx->width, pVideoCtx->height);

    //XXX should deal with case where no audio
    if (audioStream == -1)
        return -1;

    AVCodecContext *pAudioCtx = pFormatCtx->streams[audioStream]->codec;
    AVCodec *pAudioCodec = avcodec_find_decoder(pAudioCtx->codec_id);
    if (pAudioCodec == NULL)
        return -1;
    if ((r = avcodec_open(pAudioCtx, pAudioCodec)) < 0)
        return r;

    AVPacket packet;
    int frameFinished;
    while (av_read_frame(pFormatCtx, &packet) >= 0) {
        if (packet.stream_index == videoStream) {

            pVideoCtx->reordered_opaque = packet.pts;
            //XXX handle error return and cleanup (free packet etc. - same with above, need to cleanup on error)
            avcodec_decode_video2(pVideoCtx, pVideoFrame, &frameFinished, &packet);

            int64_t pts;
            if (packet.dts == AV_NOPTS_VALUE && pVideoFrame->reordered_opaque != AV_NOPTS_VALUE)
                pts = pVideoFrame->reordered_opaque;
            else if (packet.dts != AV_NOPTS_VALUE)
                pts = packet.dts;
            else
                pts = 0;

            if (frameFinished) {
                //XXX sws_scale to convert into RGB
                printf("got full video frame - packet reordered pts=%lld, pts=%lld dts=%lld\n", pts, packet.pts, packet.dts);
            }
        }
        else if (packet.stream_index == audioStream) {
            AVPacket packetTmp;
            packetTmp.data = packet.data;
            packetTmp.size = packet.size;
            while (packetTmp.size > 0) {
                DECLARE_ALIGNED(16, uint8_t, audioBuffer)[AVCODEC_MAX_AUDIO_FRAME_SIZE];
                int bufferSize = sizeof(audioBuffer);
                //XXX cleanup on error
                int bytesRead = avcodec_decode_audio3(pAudioCtx, (int16_t *)audioBuffer, &bufferSize, &packetTmp);
                if (bytesRead < 0) {
                    packetTmp.size = 0;
                    break;
                }
                packetTmp.data += bytesRead;
                packetTmp.size -= bytesRead;
                if (bufferSize <= 0)
                    continue;
                printf("decoded %d audio bytes\n", bufferSize);
                //XXX do something with decoded audio (based on pAudioCtx->channels etc.)
            }
        }

        av_free_packet(&packet);
    }

    free(videoBuffer);
    av_free(pVideoFrameRGB);
    av_free(pVideoFrame);
    avcodec_close(pVideoCtx);
    av_close_input_file(pFormatCtx);

    return 0;
}