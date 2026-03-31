#include <stdio.h>      // Librería estándar de E/S (printf, perror)
#include <stdlib.h>     // Librería estándar de utilidades
#include <string.h>     // Manipulación de memoria y cadenas (memset, strlen)
#include <unistd.h>     // Llamadas al sistema POSIX (close)
#include <sys/socket.h> // LIBRERÍA DE SOCKETS: Creación y manejo de la comunicación
#include <arpa/inet.h>  // LIBRERÍA DE RED: Estructuras y conversiones de red

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // 1. Creación del socket UDP
    // AF_INET: Direcciones IPv4 | SOCK_DGRAM: Protocolo de datagramas (UDP)
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("\nError en la creacion del socket\n");
        return -1;
    }

    // 2. Configuración de la dirección del Broker al que nos vamos a "suscribir"
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT); // Convertimos el puerto al formato de red
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP local

    // 3. El "Handshake" manual de UDP (Registro en el Broker)
    // A diferencia de TCP que usa connect(), en UDP debemos enviar proactivamente 
    // un mensaje para que el Broker conozca nuestra dirección de retorno (IP/Puerto).
    char *hello = "SUBSCRIBE";
    sendto(sock, (const char *)hello, strlen(hello), 0, 
           (const struct sockaddr *)&serv_addr, sizeof(serv_addr));

    printf("Suscrito al Broker UDP. Esperando actualizaciones...\n");
    
    // 4. Ciclo infinito de escucha
    while(1) {
        memset(buffer, 0, BUFFER_SIZE); // Limpiamos el buffer antes de cada lectura
        
        // recvfrom() es una llamada bloqueante que espera la llegada de datagramas.
        // Se diferencia de read() en TCP porque UDP necesita saber de quién viene cada paquete suelto.
        int n = recvfrom(sock, (char *)buffer, BUFFER_SIZE, 0, NULL, NULL);
        
        if (n > 0) {
            buffer[n] = '\0'; // Aseguramos el final de la cadena de texto
            printf("--- ACTUALIZACION: %s ---\n", buffer);
        } else if (n < 0) {
            perror("Error de lectura");
            break;
        }
    }

    // Cerramos el socket (Aunque en UDP no se envía paquete FIN de desconexión)
    close(sock);
    return 0;
}