/*
 * =============================================================================
 * publisher_tcp.c  –  Publicador TCP para sistema Publicador–Suscriptor
 * =============================================================================
 * DESCRIPCIÓN GENERAL:
 *   Simula un periodista deportivo que reporta eventos de un partido en vivo.
 *   Se conecta al broker mediante TCP y envía mensajes (goles, tarjetas, etc.)
 *   con un identificador de partido en cada mensaje.
 *
 * PROTOCOLO DE TRANSPORTE: TCP (SOCK_STREAM)
 *   Al llamar connect(), el kernel completa el Three-Way Handshake:
 *     1. El cliente envía SYN  (seq = ISN_cliente).
 *     2. El servidor responde  SYN-ACK (seq = ISN_servidor, ack = ISN_cliente+1).
 *     3. El cliente envía ACK  (ack = ISN_servidor+1).
 *   Solo cuando connect() retorna con éxito la conexión está ESTABLISHED.
 *
 * COMPILACIÓN (macOS / Linux):
 *   gcc -Wall -o publisher_tcp publisher_tcp.c
 *
 * USO:
 *   ./publisher_tcp <broker_ip> <partido_id>
 *   Ejemplo: ./publisher_tcp 127.0.0.1 PartidoA
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>   /* socket(), connect(), send()        */
#include <netinet/in.h>   /* struct sockaddr_in, htons()        */
#include <arpa/inet.h>    /* inet_pton() – texto → binario IP   */

#define PORT_PUB  5001    /* Debe coincidir con broker_tcp.c    */
#define BUF_SIZE  1024

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <broker_ip> <partido_id>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 PartidoA\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *broker_ip  = argv[1];
    const char *partido_id = argv[2];

    int sock_fd;
    struct sockaddr_in broker_addr;

    /*
     * PASO 1: Crear socket TCP activo
     * SOCK_STREAM crea un socket de flujo (stream), que usa TCP.
     * A diferencia del socket pasivo del broker (que llama listen()),
     * este socket se usará de forma activa (llamara connect()).
     * En este punto el socket existe solo localmente y no hay comunicación.
     */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("[PUB] Error creando socket");
        exit(EXIT_FAILURE);
    }

    /*
     * PASO 2: Configurar dirección del broker
     * inet_pton() convierte la IP en texto ("127.0.0.1") a formato binario
     * de 32 bits en Network Byte Order (big-endian).
     * htons() convierte el número de puerto al byte order de red.
     */
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port   = htons(PORT_PUB);
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) <= 0) {
        fprintf(stderr, "[PUB] Dirección IP inválida: %s\n", broker_ip);
        exit(EXIT_FAILURE);
    }

    /*
     * PASO 3: connect() – Three-Way Handshake
     * Esta llamada desencadena el proceso de establecimiento de conexión TCP:
     *
     *  Publisher (cliente)            Broker (servidor)
     *  -------------------            -----------------
     *  SYN (seq=x)       ---------->
     *                    <----------  SYN-ACK (seq=y, ack=x+1)
     *  ACK (ack=y+1)     ---------->
     *  [connect() retorna]            [accept() retorna]
     *
     * Hasta que connect() retorna con éxito, TCP no está ESTABLISHED.
     * Si el broker no está disponible, connect() falla inmediatamente
     * (RST) o después del timeout (si hay un firewall que descarta el SYN).
     *
     * EN WIRESHARK podrás ver exactamente estos 3 paquetes filtrando:
     *   tcp.port == 5001 && (tcp.flags.syn == 1 || tcp.flags.ack == 1)
     */
    printf("[PUB] Conectando al broker %s:%d...\n", broker_ip, PORT_PUB);
    if (connect(sock_fd, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("[PUB] connect() falló");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("[PUB] Conexión TCP establecida (Three-Way Handshake completo).\n");
    printf("[PUB] Publicando para partido: %s\n\n", partido_id);

    /*
     * PASO 4: Enviar mensajes
     * Enviamos 10 mensajes con eventos deportivos simulados.
     * Cada mensaje incluye el partido_id para que el broker pueda
     * (opcionalmente) filtrar por tema.
     *
     * send() copia los datos al buffer de envío del kernel; TCP se encarga
     * de empaquetarlos en segmentos, asignarles número de secuencia y
     * esperar ACK del receptor.
     *
     * El número de secuencia (SEQ) incrementa en tantos bytes como se
     * enviaron; el ACK del broker confirma cuántos bytes recibió.
     * EN WIRESHARK: columna "Seq" y "Ack" en cada paquete TCP.
     */
    const char *eventos[] = {
        "INICIO del partido",
        "Falta de Equipo B en minuto 5",
        "GOL de Equipo A al minuto 12 — 1:0",
        "Tarjeta amarilla al #7 de Equipo B (min 20)",
        "Cambio Equipo A: #10 entra por #22 (min 35)",
        "GOL de Equipo B al minuto 41 — 1:1",
        "FIN del primer tiempo",
        "INICIO segundo tiempo",
        "GOL de Equipo A al minuto 67 — 2:1",
        "FIN DEL PARTIDO — Resultado final 2:1"
    };
    int n_eventos = sizeof(eventos) / sizeof(eventos[0]);
    char msg[BUF_SIZE];

    for (int i = 0; i < n_eventos; i++) {
        /*
         * Formato del mensaje: "[<partido>] <evento>"
         * Esto permite al broker (o subscriber) filtrar por partido.
         */
        snprintf(msg, sizeof(msg), "[%s] %s", partido_id, eventos[i]);

        int n = send(sock_fd, msg, strlen(msg), 0);
        if (n < 0) {
            perror("[PUB] send");
            break;
        }
        printf("[PUB] Enviado (%d bytes): %s\n", n, msg);

        /*
         * Pausa de 1 segundo entre mensajes.
         * Esto hace visible en Wireshark el flujo de segmentos TCP
         * individuales con sus ACKs correspondientes.
         */
        sleep(1);
    }

    /*
     * PASO 5: Cierre de conexión – Four-Way Handshake
     * close() inicia el cierre ordenado de TCP:
     *
     *  Publisher                    Broker
     *  ---------                    ------
     *  FIN  ---------------------->
     *       <----------------------  ACK
     *       <----------------------  FIN  (cuando el broker también cierra)
     *  ACK  ---------------------->
     *
     * EN WIRESHARK: paquetes con flag FIN activo.
     */
    printf("\n[PUB] Todos los mensajes enviados. Cerrando conexión...\n");
    close(sock_fd);
    printf("[PUB] Conexión cerrada (FIN enviado).\n");
    return 0;
}
