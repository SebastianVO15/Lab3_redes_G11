#include <stdio.h>      // Librería de Entrada/Salida (printf, perror)
#include <stdlib.h>     // Librería de utilidades estándar (exit)
#include <string.h>     // Manipulación de memoria y cadenas (memset, strcmp)
#include <unistd.h>     // Llamadas al sistema POSIX
#include <arpa/inet.h>  // Conversiones de red (htons, inet_ntoa)
#include <sys/socket.h> // LIBRERÍA DE SOCKETS: Funciones principales de red

#define PORT 8080
#define MAX_SUBSCRIBERS 30
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUFFER_SIZE];
    
    // ARREGLO DE ESTADO MANUAL: 
    // Como UDP es stateless (no guarda conexiones), nosotros debemos guardar
    // manualmente las direcciones (IP y Puerto) de quienes se suscriben.
    struct sockaddr_in subscribers[MAX_SUBSCRIBERS];
    int num_subscribers = 0;

    // 1. Creación del socket UDP principal
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Fallo en socket");
        exit(EXIT_FAILURE);
    }

    // Limpiamos las estructuras de memoria por seguridad
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // 2. Configuración de la dirección local del Broker
    servaddr.sin_family = AF_INET;
    // INADDR_ANY indica que el Broker escuchará en todas las tarjetas de red 
    // de la máquina (ej. loopback 127.0.0.1, Ethernet, Wi-Fi).
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(PORT);

    // 3. Bind: Asocia ("amarra") el socket creado con el puerto 8080 del sistema operativo.
    // Esto es vital para un servidor, ya que los clientes necesitan un puerto fijo al que apuntar.
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Fallo en bind");
        exit(EXIT_FAILURE);
    }

    // El socket UDP ya está listo para recibir datagramas.
    printf("Broker UDP escuchando en el puerto %d\n", PORT);

    socklen_t len;
    while (1) {
        len = sizeof(cliaddr);
        memset(buffer, 0, BUFFER_SIZE);
        
        // 4. recvfrom: La función bloqueante central. 
        // Captura cualquier datagrama que llegue al puerto 8080.
        // Lo más importante: llena automáticamente la estructura 'cliaddr' con
        // la dirección IP y el puerto de origen de quien envió el mensaje.
        int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        buffer[n] = '\0';

        // 5. Lógica de Enrutamiento (Multiplexación manual)
        if (strcmp(buffer, "SUBSCRIBE") == 0) {
            // Caso A: Es un Suscriptor registrante. 
            // Guardamos su 'cliaddr' en nuestro arreglo de suscriptores.
            if (num_subscribers < MAX_SUBSCRIBERS) {
                subscribers[num_subscribers] = cliaddr;
                num_subscribers++;
                
                // inet_ntoa y ntohs convierten los datos binarios a formato humano para imprimir
                printf("Nuevo suscriptor agregado (IP: %s, Puerto: %d). Total: %d\n", 
                       inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), num_subscribers);
            } else {
                printf("Capacidad maxima de suscriptores alcanzada.\n");
            }
        } else {
            // Caso B: Es un mensaje de un Publicador. 
            // El Broker actúa como intermediario (fan-out), iterando sobre el arreglo
            // y disparando el datagrama a cada suscriptor registrado.
            printf("Broker recibio: %s\n", buffer);
            for (int i = 0; i < num_subscribers; i++) {
                sendto(sockfd, (const char *)buffer, strlen(buffer), 0, 
                       (const struct sockaddr *)&subscribers[i], sizeof(subscribers[i]));
            }
        }
    }
    return 0;
}