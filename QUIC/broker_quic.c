/*
 * broker_quic.c
 * Broker pub-sub usando QUIC simplificado sobre UDP.
 *
 * Flujo:
 *  1. Recibe PKT_INITIAL de cualquier cliente → responde PKT_HANDSHAKE
 *  2. Si el cliente envía PKT_SUBSCRIBE → lo registra como subscriber(topic)
 *  3. Si el cliente envía PKT_STREAM    → lo redistribuye a subscribers del topic
 *  4. Confirma cada paquete de datos con PKT_ACK
 *
 * Compilar:
 *   gcc -o broker_quic broker_quic.c -lpthread
 */

#include "quic.h"
#include <pthread.h>

#define MAX_SUBS 64

/* ── Estado de un subscriber ── */
typedef struct {
    int                active;
    uint32_t           conn_id;
    char               topic[64];
    struct sockaddr_in addr;
} Subscriber;

static Subscriber subs[MAX_SUBS];
static int        sub_count = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* Registra un nuevo subscriber */
static void register_subscriber(uint32_t conn_id,
                                 const char *topic,
                                 struct sockaddr_in *addr) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_SUBS; i++) {
        if (!subs[i].active) {
            subs[i].active  = 1;
            subs[i].conn_id = conn_id;
            strncpy(subs[i].topic, topic, 63);
            subs[i].addr = *addr;
            sub_count++;
            printf("[BROKER] Subscriber registrado: conn=%u topic='%s'\n",
                   conn_id, topic);
            break;
        }
    }
    pthread_mutex_unlock(&lock);
}

/* Envía el mensaje a todos los subscribers del topic */
static void broadcast(int fd, const char *topic,
                      const char *msg, uint16_t stream_id,
                      uint32_t seq) {
    QuicPacket pkt;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_SUBS; i++) {
        if (subs[i].active && strcmp(subs[i].topic, topic) == 0) {
            int len = build_packet(&pkt, PKT_STREAM,
                                   subs[i].conn_id,
                                   stream_id, seq,
                                   msg, (uint16_t)strlen(msg) + 1);
            send_packet(fd, &pkt, len, &subs[i].addr);
            printf("[BROKER] → conn=%u | stream=%u | seq=%u | %s\n",
                   subs[i].conn_id, stream_id, seq, msg);
        }
    }
    pthread_mutex_unlock(&lock);
}

/* Envía ACK al remitente */
static void send_ack(int fd, uint32_t conn_id, uint32_t seq,
                     struct sockaddr_in *dest) {
    QuicPacket ack;
    char ack_payload[16];
    snprintf(ack_payload, sizeof(ack_payload), "ACK:%u", seq);
    int len = build_packet(&ack, PKT_ACK, conn_id, 0, seq,
                           ack_payload, (uint16_t)strlen(ack_payload) + 1);
    send_packet(fd, &ack, len, dest);
}

int main(void) {
    int fd = udp_socket_bind(BROKER_PORT);
    printf("[BROKER QUIC] Escuchando en puerto %d...\n", BROKER_PORT);

    QuicPacket      pkt;
    struct sockaddr_in src;

    while (1) {
        int n = recv_packet(fd, &pkt, &src);
        if (n < 0) continue;

        uint8_t  type      = pkt.hdr.type;
        uint32_t conn_id   = pkt.hdr.conn_id;
        uint16_t stream_id = pkt.hdr.stream_id;
        uint32_t seq       = pkt.hdr.seq;

        switch (type) {

        case PKT_INITIAL: {
            /* Handshake: el cliente quiere abrir conexión */
            printf("[BROKER] INITIAL de conn=%u\n", conn_id);
            QuicPacket hs;
            const char *msg = "QUIC_HANDSHAKE_OK";
            int len = build_packet(&hs, PKT_HANDSHAKE, conn_id,
                                   0, 0, msg, (uint16_t)strlen(msg) + 1);
            send_packet(fd, &hs, len, &src);
            break;
        }

        case PKT_SUBSCRIBE: {
            /* Payload contiene el topic */
            pkt.payload[pkt.hdr.payload_len] = '\0';
            register_subscriber(conn_id, pkt.payload, &src);
            send_ack(fd, conn_id, seq, &src);
            break;
        }

        case PKT_STREAM: {
            /*
             * Payload formato: "TOPIC|mensaje"
             * stream_id identifica el "canal" del publisher
             */
            pkt.payload[pkt.hdr.payload_len] = '\0';
            char *sep = strchr(pkt.payload, '|');
            if (!sep) break;
            *sep = '\0';
            char *topic = pkt.payload;
            char *msg   = sep + 1;

            send_ack(fd, conn_id, seq, &src);
            broadcast(fd, topic, msg, stream_id, seq);
            break;
        }

        case PKT_CLOSE: {
            printf("[BROKER] CLOSE de conn=%u\n", conn_id);
            pthread_mutex_lock(&lock);
            for (int i = 0; i < MAX_SUBS; i++) {
                if (subs[i].conn_id == conn_id) {
                    subs[i].active = 0;
                    sub_count--;
                }
            }
            pthread_mutex_unlock(&lock);
            break;
        }

        default:
            printf("[BROKER] Tipo desconocido: 0x%02x\n", type);
        }
    }

    close(fd);
    return 0;
}