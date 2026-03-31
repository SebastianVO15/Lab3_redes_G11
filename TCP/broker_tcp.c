/*
 * =============================================================================
 * broker_tcp.c  –  Broker TCP para sistema Publicador–Suscriptor
 * =============================================================================
 * DESCRIPCIÓN GENERAL:
 *   Este broker actúa como intermediario central en el patrón pub-sub.
 *   Escucha en 2 puertos distintos de forma concurrente:
 *     - PORT_PUB  (5001): acepta conexiones de Publishers.
 *     - PORT_SUB  (5002): acepta conexiones de Subscribers.
 *
 *   Cuando llega un mensaje de cualquier Publisher, el broker lo reenvía
 *   inmediatamente a TODOS los Subscribers conectados en ese momento.
 *
 * PROTOCOLO DE TRANSPORTE: TCP (SOCK_STREAM)
 *   - Orientado a conexión: antes de cualquier dato se completa el
 *     Three-Way Handshake (SYN → SYN-ACK → ACK).
 *   - Confiable: cada segmento es acusado con ACK; si se pierde, se retransmite.
 *   - Ordenado: los bytes llegan en el mismo orden en que fueron enviados
 *     gracias a los números de secuencia.
 *   - Control de flujo: el campo "Window Size" evita que el emisor sature
 *     al receptor.
 *
 * CONCURRENCIA:
 *   Se usa select() + arreglos de file-descriptors para manejar múltiples
 *   conexiones sin crear hilos extra, permitiendo captura limpia en Wireshark.
 *
 * COMPILACIÓN (macOS / Linux):
 *   gcc -Wall -o broker_tcp broker_tcp.c
 *
 * USO:
 *   ./broker_tcp
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>   /* socket(), bind(), listen(), accept(), send(), recv() */
#include <netinet/in.h>   /* struct sockaddr_in, htons(), INADDR_ANY             */
#include <arpa/inet.h>    /* inet_ntoa() para mostrar IP en logs                 */
#include <sys/select.h>   /* select() – multiplexación de I/O sin hilos          */

/* Puertos */
#define PORT_PUB   5001   /* Publishers se conectan aquí  */
#define PORT_SUB   5002   /* Subscribers se conectan aquí */

/* Capacidad máxima */
#define MAX_PUBLISHERS  10
#define MAX_SUBSCRIBERS 50
#define BUF_SIZE       1024

/* Almacén de file-descriptors activos */
static int pub_fds[MAX_PUBLISHERS];
static int sub_fds[MAX_SUBSCRIBERS];
static int pub_count = 0;
static int sub_count = 0;

/* Prototipos */
static int  create_listening_socket(int port);
static void broadcast_to_subscribers(const char *msg, int len);
static void remove_fd(int *array, int *count, int idx);

/* ============================================================================
 * main()
 * ============================================================================ */
int main(void)
{
    int listen_pub, listen_sub;
    fd_set read_fds;
    int max_fd, ready, i;

    printf("[BROKER] Iniciando broker TCP\n");

    /*
     * ── PASO 1: Crear socket de escucha para Publishers ──────────────────
     * socket(AF_INET, SOCK_STREAM, 0):
     *   AF_INET     -> familia de direcciones IPv4.
     *   SOCK_STREAM -> tipo de socket orientado a conexión (TCP).
     *   0           -> el kernel elige automáticamente el protocolo TCP.
     *
     * El kernel asigna un file-descriptor (entero >= 0) que representa
     * el extremo local del socket.  En este punto el socket no está
     * asociado a ninguna dirección ni puerto.
     */
    listen_pub = create_listening_socket(PORT_PUB);
    listen_sub = create_listening_socket(PORT_SUB);

    printf("[BROKER] Escuchando publishers en puerto %d\n", PORT_PUB);
    printf("[BROKER] Escuchando subscribers en puerto %d\n", PORT_SUB);
    printf("[BROKER] Esperando conexiones...\n\n");

    /*
     * PASO 2: Bucle principal con select()
     * select() supervisa múltiples file-descriptors simultáneamente y
     * retorna cuando al menos uno está listo para leer.  Esto evita
     * bloquear en un único accept() o recv().
     */
    while (1)
    {
        /* Reiniciar el conjunto de FDs en cada iteración */
        FD_ZERO(&read_fds);

        /* Agregar los sockets de escucha pasiva */
        FD_SET(listen_pub, &read_fds);
        FD_SET(listen_sub, &read_fds);
        max_fd = (listen_pub > listen_sub) ? listen_pub : listen_sub;

        /* Agregar FDs de Publishers conectados */
        for (i = 0; i < pub_count; i++) {
            FD_SET(pub_fds[i], &read_fds);
            if (pub_fds[i] > max_fd) max_fd = pub_fds[i];
        }

        /* Agregar FDs de Subscribers conectados */
        for (i = 0; i < sub_count; i++) {
            FD_SET(sub_fds[i], &read_fds);
            if (sub_fds[i] > max_fd) max_fd = sub_fds[i];
        }

        /*
         * Bloquear hasta que algún FD esté listo (NULL = sin timeout).
         * El SO devolverá control en cuanto haya datos o una conexión nueva.
         */
        ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) {
            perror("[BROKER] select");
            break;
        }

        /* CASO A: Nueva conexión de un Publisher */
        if (FD_ISSET(listen_pub, &read_fds)) {
            /*
             * accept() extrae la conexión de la cola de backlog.
             * En este momento el Three-Way Handshake ya se completó en el
             * kernel:
             *   1. El Publisher envió SYN (seq=x).
             *   2. El broker respondió SYN-ACK (seq=y, ack=x+1).
             *   3. El Publisher envió ACK (ack=y+1).
             * accept() nos entrega un NUEVO fd para esa conexión específica.
             */
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_fd = accept(listen_pub, (struct sockaddr *)&client_addr, &addr_len);
            if (new_fd < 0) {
                perror("[BROKER] accept publisher");
            } else if (pub_count < MAX_PUBLISHERS) {
                pub_fds[pub_count++] = new_fd;
                printf("[BROKER] Publisher conectado: %s:%d (fd=%d)\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port), new_fd);
            } else {
                fprintf(stderr, "[BROKER] Capacidad máxima de publishers alcanzada.\n");
                close(new_fd);
            }
        }

        /* CASO B: Nueva conexión de un Subscriber */
        if (FD_ISSET(listen_sub, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_fd = accept(listen_sub, (struct sockaddr *)&client_addr, &addr_len);
            if (new_fd < 0) {
                perror("[BROKER] accept subscriber");
            } else if (sub_count < MAX_SUBSCRIBERS) {
                sub_fds[sub_count++] = new_fd;
                printf("[BROKER] Subscriber conectado: %s:%d (fd=%d)\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port), new_fd);
            } else {
                fprintf(stderr, "[BROKER] Capacidad máxima de subscribers alcanzada.\n");
                close(new_fd);
            }
        }

        /* CASO C: Datos de un Publisher */
        for (i = 0; i < pub_count; i++) {
            if (!FD_ISSET(pub_fds[i], &read_fds)) continue;

            char buf[BUF_SIZE];
            /*
             * recv() lee bytes del buffer TCP del kernel.
             * TCP garantiza que los bytes llegan en orden y sin duplicados
             * gracias a los números de secuencia y las ventanas deslizantes.
             * Un retorno de 0 indica que el peer cerró la conexión (FIN).
             */
            int n = recv(pub_fds[i], buf, BUF_SIZE - 1, 0);
            if (n <= 0) {
                if (n == 0)
                    printf("[BROKER] Publisher fd=%d desconectado.\n", pub_fds[i]);
                else
                    perror("[BROKER] recv publisher");
                close(pub_fds[i]);
                remove_fd(pub_fds, &pub_count, i);
                i--;  /* ajustar índice tras compactación */
                continue;
            }
            buf[n] = '\0';
            printf("[BROKER] Mensaje recibido (fd=%d): %s\n", pub_fds[i], buf);

            /* Reenviar a todos los subscribers */
            broadcast_to_subscribers(buf, n);
        }

        /* CASO D: Datos de un Subscriber */
        for (i = 0; i < sub_count; i++) {
            if (!FD_ISSET(sub_fds[i], &read_fds)) continue;
            char buf[BUF_SIZE];
            int n = recv(sub_fds[i], buf, BUF_SIZE - 1, 0);
            if (n <= 0) {
                if (n == 0)
                    printf("[BROKER] Subscriber fd=%d desconectado.\n", sub_fds[i]);
                else
                    perror("[BROKER] recv subscriber");
                close(sub_fds[i]);
                remove_fd(sub_fds, &sub_count, i);
                i--;
            }
        }
    } /* while (1) */

    close(listen_pub);
    close(listen_sub);
    return 0;
}

/* ============================================================================
 * create_listening_socket()
 * ──────────────────────────────────────────────────────────────────────────
 * Encapsula los pasos: socket -> setsockopt -> bind -> listen.
 * ============================================================================ */
static int create_listening_socket(int port)
{
    int fd;
    struct sockaddr_in addr;
    int opt = 1;

    /*
     * socket(AF_INET, SOCK_STREAM, 0)
     * --------------------------------
     * Solicita al kernel un socket TCP IPv4.
     * El kernel reserva estructuras internas (PCB – Protocol Control Block)
     * para gestionar el estado de la conexión (SYN_SENT, ESTABLISHED, etc.).
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    /*
     * setsockopt(SO_REUSEADDR)
     * ------------------------
     * Permite reutilizar el puerto inmediatamente después de cerrar el
     * proceso, evitando el error "Address already in use" que ocurre
     * durante el período TIME_WAIT de TCP (2*MSL ≈ 60 s).
     */
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /*
     * bind()
     * -------
     * Asocia el socket a una dirección IP y puerto locales.
     * INADDR_ANY  -> acepta conexiones en cualquier interfaz de red.
     * htons()     -> convierte el puerto a big-endian (Network Byte Order).
     * Tras bind(), el socket tiene una identidad (IP:puerto) pero todavía
     * no acepta conexiones entrantes.
     */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }

    /*
     * listen(fd, backlog=10)
     * -----------------------
     * Marca el socket como PASIVO: dispuesto a aceptar conexiones.
     * El parámetro backlog (10) define el tamaño de la cola de conexiones
     * completadas (ya con el handshake) pero todavía no aceptadas con accept().
     * A partir de aquí el kernel responde automáticamente a los SYN
     * entrantes completando el Three-Way Handshake.
     */
    if (listen(fd, 10) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }
    return fd;
}

/* ============================================================================
 * broadcast_to_subscribers()
 * ──────────────────────────────────────────────────────────────────────────
 * Envía 'msg' (longitud 'len') a cada subscriber conectado.
 * ============================================================================ */
static void broadcast_to_subscribers(const char *msg, int len)
{
    int i;
    for (i = 0; i < sub_count; i++) {
        /*
         * send() entrega los bytes al buffer de envío TCP del kernel.
         * El kernel se encarga de segmentar, numerar y retransmitir si
         * es necesario.  El campo "Window Size" del encabezado TCP
         * refleja cuántos bytes puede recibir el extremo remoto sin
         * que el buffer se desborde (control de flujo).
         */
        int sent = send(sub_fds[i], msg, len, 0);
        if (sent < 0) {
            perror("[BROKER] send subscriber");
        }
    }
    printf("[BROKER] Mensaje broadcast a %d subscriber(s).\n\n", sub_count);
}

/* ============================================================================
 * remove_fd()
 * ──────────────────────────────────────────────────────────────────────────
 * Elimina el elemento en 'idx' del arreglo compactando hacia la izquierda.
 * ============================================================================ */
static void remove_fd(int *array, int *count, int idx)
{
    array[idx] = array[*count - 1];
    (*count)--;
}
