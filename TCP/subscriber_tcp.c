#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0, valread;
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

    // Connect: Establece el canal confiable mediante el Handshake de 3 vías.
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConexion fallida. Asegurese de que el Broker este ejecutandose.\n");
        return -1;
    }

    printf("Suscrito al Broker. Esperando actualizaciones...\n");

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        // Read: Es una llamada bloqueante. TCP asegura que los datos se entreguen en orden 
        valread = read(sock, buffer, BUFFER_SIZE);
        
        if (valread == 0) {
            printf("El Broker ha cerrado la conexion.\n");
            break;
        } else if (valread < 0) {
            perror("Error de lectura");
            break;
        }

        printf("--- ACTUALIZACION: %s ---\n", buffer);
    }

    close(sock);
    return 0;
}