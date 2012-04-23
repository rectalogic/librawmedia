#include "packet_queue.h"

void packet_queue_init(PacketQueue* q) {
    memset(q, 0, sizeof(PacketQueue));
}

int packet_queue_put(PacketQueue* q, AVPacket* pkt) {
    AVPacketList* pktl;

    if (av_dup_packet(pkt) < 0)
        return -1;
    pktl = av_malloc(sizeof(AVPacketList));
    if (!pktl)
        return -1;
    pktl->pkt = *pkt;
    pktl->next = NULL;

    if (!q->last_pkt)
        q->first_pkt = pktl;
    else
        q->last_pkt->next = pktl;
    q->last_pkt = pktl;
    return 0;
}

// Return 0 if no packet, 1 if packet
int packet_queue_get(PacketQueue* q, AVPacket* pkt) {
    AVPacketList* pktl = q->first_pkt;
    if (pktl) {
        q->first_pkt = pktl->next;
        if (!q->first_pkt)
            q->last_pkt = NULL;
        *pkt = pktl->pkt;
        av_free(pktl);
        return 1;
    }
    return 0;
}

void packet_queue_flush(PacketQueue* q) {
    AVPacketList* pkt;
    AVPacketList* pktl;
    for (pkt = q->first_pkt; pkt != NULL; pkt = pktl) {
        pktl = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
}

