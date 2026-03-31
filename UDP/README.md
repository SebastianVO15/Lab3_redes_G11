# Laboratorio 3: Sistema de Publicación-Suscripción (UDP)

**Implementación:** Protocolo UDP (User Datagram Protocol)

Este proyecto implementa un sistema de mensajería bajo el patrón de Publicación-Suscripción utilizando sockets no orientados a la conexión (UDP) en lenguaje C. El sistema está compuesto por un Broker central, múltiples Suscriptores y múltiples Publicadores.

---

## 🛠️ Instrucciones de Compilación

Para compilar los archivos fuente, es necesario utilizar el compilador `gcc` en un entorno Linux (nativo, máquina virtual o WSL). Abra una terminal en el directorio del proyecto y ejecute los siguientes comandos:

```bash
gcc broker_udp.c -o broker
gcc subscriber_udp.c -o suscriptor
gcc publisher_udp.c -o publicador


## 🛠️ Instrucciones de Compilación

Para compilar los archivos fuente, es necesario utilizar el compilador `gcc` en un entorno Linux (nativo, máquina virtual o WSL). Abra una terminal en el directorio del proyecto y ejecute los siguientes comandos:

```bash
gcc broker_udp.c -o broker
gcc subscriber_udp.c -o suscriptor
gcc publisher_udp.c -o publicador


🚀 Instrucciones de Ejecución
Para evaluar el sistema correctamente, se deben utilizar múltiples terminales simultáneas.

Prueba Estándar 
Iniciar el Broker: En la primera terminal, levante el servidor intermediario.

./broker
(El Broker indicará que está escuchando en el puerto 8080).

Conectar Suscriptores: En dos (o más) terminales nuevas, inicie los clientes que recibirán la información.

Bash
./suscriptor
(Cada suscriptor enviará automáticamente un datagrama de registro al Broker).

Conectar Publicador(es): En otra terminal, inicie el publicador.

Bash
./publicador
Escriba los mensajes (ej. "Gol al minuto 10") y presione Enter. Verifique que los mensajes se replican instantáneamente en las terminales de los suscriptores. Para salir, escriba salir.

Prueba de Estrés 
Para comprobar empíricamente la falta de confiabilidad de UDP, el archivo publisher_udp.c incluye un bloque de código comentado que envía una ráfaga de 50,000 datagramas.

Abra publisher_udp.c y comente el ciclo while(1) del "Modo Manual".

Descomente el bloque "Modo Prueba de Estrés".

Vuelva a compilar (gcc publisher_udp.c -o publicador).

Ejecute el sistema y observe en las terminales de los suscriptores cómo la secuencia lógica de los mensajes se rompe debido al descarte de paquetes en el kernel.



📚 Documentación y Justificación de Librerías (Req. 6.2)
En estricto cumplimiento con los lineamientos del laboratorio, no se utilizaron librerías externas de terceros. El sistema fue construido íntegramente utilizando la interfaz estándar de Berkeley Sockets de los sistemas POSIX.

A continuación, se detalla la interacción del código con cada función utilizada de las librerías nativas <sys/socket.h> y <arpa/inet.h>:

1. Funciones de Red (Librería <sys/socket.h>)
Esta librería provee las estructuras y funciones base para la comunicación a nivel del sistema operativo.

socket(AF_INET, SOCK_DGRAM, 0): Utilizada en los tres componentes (Broker, Publicador, Suscriptor). Solicita al kernel la creación de un endpoint de comunicación. Se especifica AF_INET para usar la familia de protocolos IPv4 y SOCK_DGRAM para forzar el uso de datagramas, mapeando la comunicación al protocolo no orientado a conexión UDP.

bind(...): Exclusiva del Broker. Asocia el socket creado con el puerto estático 8080 del sistema operativo. A diferencia de TCP, tras realizar el bind(), no se invocan listen() ni accept(), ya que UDP no mantiene colas de sesiones.

recvfrom(...): Utilizada por el Broker y los Suscriptores. Es una llamada bloqueante que extrae datagramas entrantes. En el Broker, cumple un propósito crítico adicional: extrae la dirección IP y puerto de origen desde la cabecera IP/UDP y la almacena en la estructura cliaddr, permitiendo registrar manualmente a los suscriptores.

sendto(...): Utilizada por el Publicador y el Broker. Al no existir un canal persistente , sendto() requiere que se le pase explícitamente la estructura de destino (sockaddr_in) en cada invocación para enrutar el datagrama individual a través de la red.

2. Transformaciones de Red (Librería <arpa/inet.h>)
Encargada de las conversiones de datos entre la arquitectura local de la máquina y los estándares de internet.

htons(...): (Host TO Network Short). Convierte el número de puerto (ej. 8080) del formato Little-Endian del procesador local al formato Big-Endian exigido por el estándar de red.

inet_addr(...): Convierte la dirección IP en formato de cadena de texto (ej. "127.0.0.1") a su representación binaria nativa de 32 bits requerida para la capa de red.

inet_ntoa(...) y ntohs(...): Utilizadas por el Broker para el proceso inverso (Network to ASCII y Network TO Host Short). Permiten traducir los datos binarios de los clientes entrantes a texto legible para imprimirlos en los logs de la consola.

INADDR_ANY: Macro utilizada en el Broker para enlazar el socket servidor a todas las interfaces de red disponibles en la máquina huésped, no solo a la interfaz loopback.

3. Librerías Estándar de C (<string.h>, <unistd.h>)
memset(...): Interacción directa con la memoria para inicializar a cero las estructuras sockaddr_in y los buffers de recepción, previniendo el envío o lectura de basura residual de memoria.

close(...): Llamada al sistema POSIX para liberar el descriptor de archivo local. Al utilizar UDP, esta llamada no genera tráfico de red (no envía paquetes FIN/ACK), simplemente destruye el endpoint a nivel del sistema operativo local.