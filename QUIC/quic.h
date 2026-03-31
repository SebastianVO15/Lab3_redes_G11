#ifndef QUIC_H
#define QUIC_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>

#define BROKER_PORT   9000
#define BUF_SIZE      1200

#define PKT_INITIAL   0x01
#define PKT_HANDSHAKE 0x02
#define PKT_STREAM    0x03
#define PKT_ACK       0x04
#define PKT_SUBSCRIBE 0x05
#define PKT_CLOSE     0x06

#pragma pack(push, 1)
typedef struct {
    uint8_t  type;
    uint32_t conn_id;
    uint16_t stream_id;
    uint32_t seq;
    uint16_t payload_len;
} QuicHeader;
#pragma pack(pop)

#define HEADER_SIZE sizeof(QuicHeader)
#define MAX_PAYLOAD (BUF_SIZE - HEADER_SIZE)

typedef struct {
    QuicHeader hdr;
    char       payload[MAX_PAYLOAD];
} QuicPacket;

static inline int build_packet(QuicPacket *pkt,
                                uint8_t type, uint32_t conn_id,
                                uint16_t stream_id, uint32_t seq,
                                const char *payload, uint16_t plen) {
    pkt->hdr.type        = type;
    pkt->hdr.conn_id     = htonl(conn_id);
    pkt->hdr.stream_id   = htons(stream_id);
    pkt->hdr.seq         = htonl(seq);
    pkt->hdr.payload_len = htons(plen);
    if (plen > 0 && payload)
        memcpy(pkt->payload, payload, plen);
    return (int)(HEADER_SIZE + plen);
}

static inline void decode_header(QuicHeader *h) {
    h->conn_id     = ntohl(h->conn_id);
    h->stream_id   = ntohs(h->stream_id);
    h->seq         = ntohl(h->seq);
    h->payload_len = ntohs(h->payload_len);
}

static inline uint32_t new_conn_id(void) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    return (uint32_t)rand();
}

static inline int udp_socket_bind(int port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(port)
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    return fd;
}

static inline int send_packet(int fd, const QuicPacket *pkt,
                               int total_len,
                               const struct sockaddr_in *dest) {
    return sendto(fd, pkt, total_len, 0,
                  (const struct sockaddr *)dest, sizeof(*dest));
}

static inline int recv_packet(int fd, QuicPacket *pkt,
                               struct sockaddr_in *src) {
    socklen_t slen = sizeof(*src);
    int n = recvfrom(fd, pkt, sizeof(*pkt), 0,
                     (struct sockaddr *)src, &slen);
    if (n < (int)HEADER_SIZE) return -1;
    decode_header(&pkt->hdr);
    return n;
}

#endif