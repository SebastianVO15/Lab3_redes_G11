/*
 * =============================================================================
 * subscriber_tcp.c  –  Suscriptor TCP para sistema Publicador–Suscriptor
 * =============================================================================
 * DESCRIPCIÓN GENERAL:
 *   Simula un aficionado que se conecta al broker para recibir en tiempo real
 *   las actualizaciones de los partidos de fútbol.
 *
 *   El subscriber se conecta al puerto de subscribers del broker (5002),
 *   y permanece en un bucle de recv() esperando mensajes indefinidamente.
 *   Imprime cada mensaje recibido con su timestamp local.
 *
 * PROTOCOLO DE TRANSPORTE: TCP (SOCK_STREAM)
 *   La confiabilidad de TCP garantiza que ningún mensaje se pierde ni llega
 *   duplicado o desordenado.  Esto es observable en Wireshark:
 *     - Cada segmento tiene un número de secuencia (SEQ).
 *     - El receptor confirma con ACK = SEQ_recibido + bytes_recibidos.
 *     - Si un ACK no llega, el emisor retransmite (visible como paquete
 *       duplicado con la misma SEQ en Wireshark).
 *
 * COMPILACIÓN (macOS / Linux):
 *   gcc -Wall -o subscriber_tcp subscriber_tcp.c
 *
 * USO:
 *   ./subscriber_tcp <broker_ip>
 *   Ejemplo: ./subscriber_tcp 127.0.0.1
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_SUB  5002    /* Puerto de subscribers en broker_tcp.c */
#define BUF_SIZE  1024

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <broker_ip>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *broker_ip = argv[1];
    int sock_fd;
    struct sockaddr_in broker_addr;
    char buf[BUF_SIZE];

    /*
     * PASO 1: Crear socket TCP activo
     * Idéntico al publisher: SOCK_STREAM -> TCP.
     * El subscriber también inicia la conexión (rol ACTIVO / cliente).
     */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("[SUB] Error creando socket");
        exit(EXIT_FAILURE);
    }

    /*
     * PASO 2: Configurar dirección del broker
     * Apuntamos al puerto de subscribers (5002).
     */
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port   = htons(PORT_SUB);
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) <= 0) {
        fprintf(stderr, "[SUB] Dirección IP inválida: %s\n", broker_ip);
        exit(EXIT_FAILURE);
    }

    /*
     * PASO 3: connect() – Three-Way Handshake ───────────────────────────
     * El subscriber dispara el handshake hacia el puerto 5002 del broker.
     *
     *  Subscriber (cliente)           Broker (servidor, puerto 5002)
     *  ---------------------          ------------------------------
     *  SYN (seq=a)       ----------->
     *                    <-----------  SYN-ACK (seq=b, ack=a+1)
     *  ACK (ack=b+1)     ----------->
     *  [connect() retorna]            [accept() retorna en broker]
     *
     * EN WIRESHARK (filtro): tcp.port == 5002
     *   Observa los 3 primeros paquetes con flags SYN, SYN-ACK, ACK.
     *   Estos son la "prueba" de que TCP es orientado a conexión.
     */
    printf("[SUB] Conectando al broker %s:%d...\n", broker_ip, PORT_SUB);
    if (connect(sock_fd, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("[SUB] connect() falló");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SUB] Conexión TCP establecida. Esperando mensajes...\n");
    printf("[SUB] (Presiona Ctrl+C para salir)\n\n");

    int msg_count = 0;

    /*
     * PASO 4: Bucle de recepción
     * recv() bloquea hasta que haya datos en el buffer TCP del kernel.
     *
     * ORDEN GARANTIZADO:
     *   TCP usa números de secuencia para reensamblar los segmentos en el
     *   orden correcto antes de entregarlos a la aplicación.  Aunque los
     *   segmentos lleguen desordenados a nivel IP, recv() nunca devuelve
     *   bytes fuera de orden.
     *
     * EN WIRESHARK:
     *   • Columna "Seq": número de secuencia del primer byte del segmento.
     *   • Columna "Ack": byte que el receptor espera recibir a continuación.
     *   • Columna "Len": cantidad de bytes de datos en el segmento.
     *   • Si Seq de un paquete = Seq_anterior + Len_anterior → orden correcto.
     *   • "Window Size" (Win): cuántos bytes más puede recibir el buffer
     *     del subscriber sin desbordarse (control de flujo).
     */
    while (1) {
        int n = recv(sock_fd, buf, BUF_SIZE - 1, 0);

        if (n < 0) {
            perror("[SUB] recv");
            break;
        }

        if (n == 0) {
            /*
             * recv() retorna 0 cuando el peer (broker) cerró la conexión
             * con un segmento FIN.  Equivale a EOF en un archivo.
             */
            printf("[SUB] El broker cerró la conexión (FIN recibido).\n");
            break;
        }

        buf[n] = '\0';
        msg_count++;

        /*
         * Mostrar timestamp local para evidenciar el orden de llegada.
         * Como TCP garantiza el orden, msg_count debe incrementar de 1 en 1
         * y los mensajes deben aparecer en la misma secuencia que el
         * publisher los envió.
         */
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        printf("[SUB][%02d:%02d:%02d] Mensaje #%d (%d bytes): %s\n",
               t->tm_hour, t->tm_min, t->tm_sec,
               msg_count, n, buf);
    }

    /*
     * PASO 5: Cierre de conexión
     * close() envía FIN al broker, completando el cierre ordenado TCP.
     * EN WIRESHARK: paquete con flag FIN=1 desde el subscriber.
     */
    printf("\n[SUB] Total mensajes recibidos: %d\n", msg_count);
    close(sock_fd);
    printf("[SUB] Socket cerrado.\n");
    return 0;
}
