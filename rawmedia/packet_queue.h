#ifndef RM_PACKET_QUEUE_H
#define RM_PACKET_QUEUE_H

#include "exports.h"

#include <libavformat/avformat.h>

typedef struct PacketQueue {
    AVPacketList* first_pkt;
    AVPacketList* last_pkt;
} PacketQueue;

RAWMEDIA_LOCAL void packet_queue_init(PacketQueue* q);
RAWMEDIA_LOCAL int packet_queue_put(PacketQueue* q, AVPacket* pkt);
RAWMEDIA_LOCAL int packet_queue_get(PacketQueue* q, AVPacket* pkt);
RAWMEDIA_LOCAL void packet_queue_flush(PacketQueue* q);

#endif
