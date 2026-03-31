#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

int main() {
    int master_socket, addrlen, new_socket, client_socket[MAX_CLIENTS], max_clients = MAX_CLIENTS, activity, i, valread, sd;
    int max_sd;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE];
    fd_set readfds; // Conjunto de descriptores de archivo para select()

    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }

    // 1. Creación del socket: AF_INET (IPv4), SOCK_STREAM (TCP)
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // master_socket es el socket que escucha nuevas conexiones
        perror("Fallo en socket");
        exit(EXIT_FAILURE);
    }

    // ROBUSTEZ: SO_REUSEADDR permite liberar el puerto inmediatamente tras cerrar el programa, evitando el error "Address already in use"
    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("Fallo en setsockopt");
        exit(EXIT_FAILURE);
    }

    // 2. Definir la dirección del socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Escucha en todas las interfaces de red locales
    address.sin_port = htons(PORT);       // Convierte el puerto al formato de red (Big Endian)

    // 3. Bind: Asocia el socket creado con la IP y el Puerto definidos
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Fallo en bind");
        exit(EXIT_FAILURE);
    }

    // 4. Listen: Pone el socket en modo pasivo, esperando a que los clientes inicien la conexión (SYN)
    if (listen(master_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(address);
    printf("Broker TCP escuchando en el puerto %d\n", PORT);

    while (1) {
        // Limpiar el conjunto de sockets y añadir el socket maestro
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Añadir sockets de clientes válidos al conjunto
        for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Esperar actividad en alguno de los sockets
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        // Si hay actividad en el master_socket, es una nueva conexión entrante
        if (FD_ISSET(master_socket, &readfds)) {
            // 5. Accept: Completa el 3-way handshake a nivel del OS. Extrae la conexión de la cola y crea un nuevo socket para este cliente específico
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            printf("Nueva conexion: File Descriptor es %d, IP es %s, Puerto %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Añadir el nuevo socket al array de clientes
            for (i = 0; i < max_clients; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }

        // Revisar todos los clientes para ver si enviaron datos (Publicadores)
        for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];
            if (FD_ISSET(sd, &readfds)) {
                // Leer el mensaje
                if ((valread = read(sd, buffer, BUFFER_SIZE)) == 0) {
                    // El cliente se desconectó (Envió un paquete FIN)
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Cliente desconectado: IP %s, Puerto %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    close(sd); // Liberar recursos
                    client_socket[i] = 0;
                } else {
                    // Se recibió un mensaje. El broker actúa como distribuidor
                    buffer[valread] = '\0';
                    printf("Broker recibio: %s\n", buffer);
                    
                    // Retransmitir a TODOS los demás clientes conectados (Suscriptores)
                    for (int j = 0; j < max_clients; j++) {
                        if (client_socket[j] != 0 && client_socket[j] != sd) {
                            send(client_socket[j], buffer, strlen(buffer), 0);
                        }
                    }
                }
            }
        }
    }
    return 0;
}