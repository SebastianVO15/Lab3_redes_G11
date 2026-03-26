/*
 * subscriber_quic.c
 * Suscriptor de eventos sobre QUIC simplificado.
 *
 * Uso: ./subscriber_quic <topic>
 * Ejemplo: ./subscriber_quic "PartidoA"
 *
 * Compilar:
 *   gcc -o subscriber_quic subscriber_quic.c
 */

#include "quic.h"

#define BROKER_HOST "127.0.0.1"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <topic>\n", argv[0]);
        return 1;
    }
    const char *topic   = argv[1];
    uint32_t    conn_id = new_conn_id();

    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    /* Ligar a puerto efímero para recibir */
    struct sockaddr_in local = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(0)   /* el SO asigna puerto libre */
    };
    bind(fd, (struct sockaddr *)&local, sizeof(local));

    struct sockaddr_in broker_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(BROKER_PORT)
    };
    inet_pton(AF_INET, BROKER_HOST, &broker_addr.sin_addr);

    /* ── 1. Handshake ── */
    QuicPacket pkt;
    int len = build_packet(&pkt, PKT_INITIAL, conn_id, 0, 0,
                           "HELLO", 6);
    sendto(fd, &pkt, len, 0,
           (struct sockaddr *)&broker_addr, sizeof(broker_addr));

    struct sockaddr_in from;
    recv_packet(fd, &pkt, &from);
    if (pkt.hdr.type != PKT_HANDSHAKE) {
        fprintf(stderr, "[SUB] Handshake fallido\n");
        return 1;
    }
    printf("[SUB] Handshake OK. conn_id=%u suscribiéndose a '%s'\n",
           conn_id, topic);

    /* ── 2. Suscripción ── */
    len = build_packet(&pkt, PKT_SUBSCRIBE, conn_id, 0, 0,
                       topic, (uint16_t)strlen(topic) + 1);
    sendto(fd, &pkt, len, 0,
           (struct sockaddr *)&broker_addr, sizeof(broker_addr));

    /* Esperar ACK de suscripción */
    recv_packet(fd, &pkt, &from);
    if (pkt.hdr.type == PKT_ACK)
        printf("[SUB] Suscripción confirmada.\n");

    /* ── 3. Recibir eventos ── */
    printf("[SUB] Esperando eventos del partido '%s'...\n\n", topic);

    uint32_t last_seq = UINT32_MAX;   /* para detectar desorden */

    while (1) {
        int n = recv_packet(fd, &pkt, &from);
        if (n < 0) continue;

        if (pkt.hdr.type == PKT_STREAM) {
            pkt.payload[pkt.hdr.payload_len] = '\0';
            uint32_t seq = pkt.hdr.seq;

            /* Detección de paquete desordenado */
            if (last_seq != UINT32_MAX && seq != last_seq + 1)
                printf("[SUB] ⚠ Paquete fuera de orden: esperado %u, recibido %u\n",
                       last_seq + 1, seq);
            last_seq = seq;

            printf("[SUB] [stream=%u][seq=%u] %s\n",
                   pkt.hdr.stream_id, seq, pkt.payload);
        } else if (pkt.hdr.type == PKT_CLOSE) {
            printf("[SUB] Conexión cerrada por el broker.\n");
            break;
        }
    }

    close(fd);
    return 0;
}