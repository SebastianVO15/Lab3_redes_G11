#include <stdio.h>      // Librería estándar de E/S (printf, perror, fgets)
#include <stdlib.h>     // Librería estándar de utilidades
#include <string.h>     // Manipulación de cadenas y memoria (memset, strlen, strcmp)
#include <unistd.h>     // Llamadas al sistema POSIX (close)
#include <sys/socket.h> // LIBRERÍA DE SOCKETS: Fundamental para crear/comunicar sockets
#include <arpa/inet.h>  // LIBRERÍA DE RED: Manipulación de direcciones IP y puertos

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // 1. Creación del socket usando <sys/socket.h>
    // AF_INET: Dominio IPv4. SOCK_DGRAM: Tipo de socket para datagramas (UDP). 0: Protocolo por defecto (UDP).
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("\nError en la creacion del socket\n");
        return -1;
    }

    // 2. Configuración de la dirección del servidor (Broker) usando <arpa/inet.h>
    memset(&serv_addr, 0, sizeof(serv_addr)); // Limpiamos la estructura en memoria
    serv_addr.sin_family = AF_INET;           // Asignamos la familia de direcciones (IPv4)
    serv_addr.sin_port = htons(PORT);         // htons() convierte el puerto al formato Big-Endian de red
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // inet_addr() convierte la IP de texto a binario

    printf("Publicador UDP listo.\n");
    printf("1. Escriba un mensaje para enviar.\n");
    printf("2. Escriba 'salir' para terminar.\n");
    printf("(Nota: El codigo incluye una prueba de estres comentada para evaluar perdida de paquetes)\n\n");
    
    // --- OPCIÓN 1: MODO MANUAL  ---
    while(1) {
        printf("Publicar > ");
        fgets(buffer, BUFFER_SIZE, stdin); // Leemos la entrada del usuario
        
        // Limpiamos el salto de línea generado por presionar Enter
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strcmp(buffer, "salir") == 0) {
            break; // Salimos del ciclo para cerrar el programa
        }
        
        // sendto() inyecta el datagrama en la red hacia la IP y Puerto definidos en serv_addr
        sendto(sock, (const char *)buffer, strlen(buffer), 0, 
               (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    }

    /* // --- OPCIÓN 2: MODO PRUEBA DE ESTRÉS (Descomentar para evaluar el Buffer Overflow) ---
    // Este bloque envía 50,000 datagramas de forma ininterrumpida para saturar el broker.
    // Demuestra que UDP no tiene control de flujo ni ACKs, provocando pérdida de paquetes.
    
    printf("Iniciando ráfaga de 50,000 mensajes...\n");
    for(int i = 1; i <= 50000; i++) {
        sprintf(buffer, "Mensaje numero %d", i);
        sendto(sock, (const char *)buffer, strlen(buffer), 0, 
               (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    }
    printf("Rafaga terminada.\n");
    */

    // 3. Cierre del socket usando <unistd.h>
    // Libera el descriptor de archivo, pero NO envía un paquete de desconexión (FIN) en la red, ya que UDP es stateless.
    close(sock);
    return 0;
}