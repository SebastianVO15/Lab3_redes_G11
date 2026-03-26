#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // Creación del socket TCP
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Error en la creacion del socket \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nDireccion invalida / No soportada \n");
        return -1;
    }

    // Connect: Aquí inicia el TCP Three-Way Handshake (SYN ->, <- SYN-ACK, ACK ->) 
    // Establece el canal confiable antes de enviar cualquier dato.
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConexion fallida. Asegurese de que el Broker este ejecutandose.\n");
        return -1;
    }

    printf("Conectado al Broker. Escriba las actualizaciones a publicar. Escriba 'salir' para terminar.\n");

    while(1) {
        printf("Publicar > ");
        fgets(buffer, BUFFER_SIZE, stdin);
        
        // Limpiar el salto de línea
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "salir") == 0) {
            break;
        }

        // Send: TCP garantiza que este mensaje llegue al destino (Broker) manejando ACKs internos 
        send(sock, buffer, strlen(buffer), 0);
    }

    close(sock); // Envía paquete FIN para terminar la conexión
    return 0;
}