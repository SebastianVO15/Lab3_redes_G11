#include "quic.h"

#define BROKER_HOST "127.0.0.1"
#define TIMEOUT_MS  500
#define MAX_RETRIES 5

static int fd;
static struct sockaddr_in broker_addr;

static int send_reliable(QuicPacket *pkt, int pkt_len, uint32_t expected_seq) {
    struct timeval tv = { .tv_sec = 0, .tv_usec = TIMEOUT_MS * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        send_packet(fd, pkt, pkt_len, &broker_addr);

        QuicPacket ack;
        struct sockaddr_in from;
        int n = recv_packet(fd, &ack, &from);

        if (n > 0 && ack.hdr.type == PKT_ACK &&
            ack.hdr.seq == expected_seq) {
            printf("[PUB] ACK recibido para seq=%u\n", expected_seq);
            return 0;
        }
        printf("[PUB] Timeout/NAK, reintentando (%d/%d)...\n",
               attempt + 1, MAX_RETRIES);
    }
    fprintf(stderr, "[PUB] ERROR: no se recibió ACK tras %d intentos\n",
            MAX_RETRIES);
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <topic> <stream_id>\n", argv[0]);
        return 1;
    }
    const char  *topic     = argv[1];
    uint16_t     stream_id = (uint16_t)atoi(argv[2]);
    uint32_t     conn_id   = new_conn_id();

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    broker_addr = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port   = htons(BROKER_PORT)
    };
    inet_pton(AF_INET, BROKER_HOST, &broker_addr.sin_addr);

    QuicPacket pkt;
    int len = build_packet(&pkt, PKT_INITIAL, conn_id, 0, 0,
                           "HELLO", 6);
    send_packet(fd, &pkt, len, &broker_addr);

    struct sockaddr_in from;
    recv_packet(fd, &pkt, &from);
    if (pkt.hdr.type != PKT_HANDSHAKE) {
        fprintf(stderr, "[PUB] Handshake fallido\n");
        return 1;
    }
    printf("[PUB] Handshake OK. conn_id=%u stream_id=%u topic='%s'\n",
           conn_id, stream_id, topic);

    const char *events[] = {
        "Inicio del partido",
        "Gol de Equipo A al minuto 12",
        "Tarjeta amarilla al #5 de Equipo B",
        "Gol de Equipo B al minuto 28",
        "Cambio: jugador 10 por jugador 22",
        "Gol de Equipo A al minuto 45 — ¡Fin del primer tiempo!",
        "Inicio del segundo tiempo",
        "Tarjeta roja al #3 de Equipo A",
        "Gol de Equipo B al minuto 78 — ¡Empate!",
        "Pitazo final — Resultado: 2-2"
    };

    for (int i = 0; i < 10; i++) {
        char payload[MAX_PAYLOAD];
        snprintf(payload, sizeof(payload), "%s|%s", topic, events[i]);

        len = build_packet(&pkt, PKT_STREAM, conn_id, stream_id,
                           (uint32_t)i,
                           payload, (uint16_t)strlen(payload) + 1);

        printf("[PUB] Enviando seq=%d: %s\n", i, events[i]);
        send_reliable(&pkt, len, (uint32_t)i);
        sleep(1);
    }

    len = build_packet(&pkt, PKT_CLOSE, conn_id, 0, 0, "BYE", 4);
    send_packet(fd, &pkt, len, &broker_addr);
    printf("[PUB] Conexión cerrada.\n");

    close(fd);
    return 0;
}