#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

#include <libavformat/avformat.h>

typedef struct PacketQueue {
    AVPacketList* first_pkt;
    AVPacketList* last_pkt;
} PacketQueue;

void packet_queue_init(PacketQueue* q);
int packet_queue_put(PacketQueue* q, AVPacket* pkt);
int packet_queue_get(PacketQueue* q, AVPacket* pkt);
void packet_queue_flush(PacketQueue* q);

#endif
