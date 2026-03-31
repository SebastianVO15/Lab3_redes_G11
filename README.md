# Laboratorio 3: Sistema de Publicación-Suscripción (UDP)

**Implementación:** Protocolo UDP (User Datagram Protocol)

Este proyecto implementa un sistema de mensajería bajo el patrón de Publicación-Suscripción utilizando sockets no orientados a la conexión (UDP) en lenguaje C. El sistema está compuesto por un Broker central, múltiples Suscriptores y múltiples Publicadores.

---

## Instrucciones de Compilación

Para compilar los archivos fuente, es necesario utilizar el compilador `gcc` en un entorno Linux (nativo, máquina virtual o WSL). Abra una terminal en el directorio del proyecto y ejecute los siguientes comandos:

```bash
gcc broker_udp.c -o broker
gcc subscriber_udp.c -o suscriptor
gcc publisher_udp.c -o publicador


## Instrucciones de Compilación

Para compilar los archivos fuente, es necesario utilizar el compilador `gcc` en un entorno Linux (nativo, máquina virtual o WSL). Abra una terminal en el directorio del proyecto y ejecute los siguientes comandos:

```bash
gcc broker_udp.c -o broker
gcc subscriber_udp.c -o suscriptor
gcc publisher_udp.c -o publicador


Instrucciones de Ejecución
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

---

## Documentación y Justificación de Librerías
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

---

# Laboratorio 3: Sistema de Publicación-Suscripción (TCP)

**Implementación:** Protocolo TCP (Transmission Control Protocol)

Este proyecto implementa un sistema de mensajería bajo el patrón de Publicación-Suscripción utilizando sockets orientados a la conexión (TCP) en lenguaje C. El sistema está compuesto por un Broker central, múltiples Suscriptores y múltiples Publicadores. A diferencia de UDP, cada componente establece una conexión persistente con el Broker mediante el Three-Way Handshake antes de intercambiar datos, garantizando entrega confiable y orden en los mensajes.

---

## Instrucciones de Compilación

Para compilar los archivos fuente, es necesario utilizar el compilador `gcc` en un entorno Linux o macOS. Abra una terminal en el directorio del proyecto y ejecute los siguientes comandos:

```bash
gcc -Wall -o broker_tcp     broker_tcp.c
gcc -Wall -o publisher_tcp  publisher_tcp.c
gcc -Wall -o subscriber_tcp subscriber_tcp.c
```

La flag `-Wall` activa todos los avisos del compilador, recomendada para verificar la calidad del código durante el desarrollo.

---

## Instrucciones de Ejecución

Para evaluar el sistema correctamente, se deben utilizar **cinco terminales simultáneas**. El orden de arranque es crítico en TCP: el Broker debe estar en estado `LISTEN` antes de que cualquier cliente intente conectarse, ya que `connect()` fallará inmediatamente si no hay un servidor esperando.

### Prueba estándar (1 broker · 2 suscriptores · 2 publicadores)

**Paso 1 — Iniciar el Broker (Terminal 1):**

```bash
./broker_tcp
```

El Broker abrirá dos sockets pasivos simultáneamente:
- Puerto **5001**: escucha conexiones de Publicadores.
- Puerto **5002**: escucha conexiones de Suscriptores.

Indicará en consola que está esperando conexiones en ambos puertos.

**Paso 2 — Conectar los Suscriptores (Terminales 2 y 3):**

Se recomienda conectar los suscriptores *antes* que los publicadores para evitar perder mensajes iniciales.

```bash
# Terminal 2
./subscriber_tcp 127.0.0.1

# Terminal 3
./subscriber_tcp 127.0.0.1
```

Al ejecutarse, cada suscriptor completa el Three-Way Handshake con el Broker (SYN → SYN-ACK → ACK) y queda bloqueado en `recv()`, esperando mensajes en tiempo real.

**Paso 3 — Conectar los Publicadores (Terminales 4 y 5):**

Cada publicador recibe como argumento la IP del Broker y un identificador de partido. Esto permite que los suscriptores distingan de qué partido proviene cada mensaje.

```bash
# Terminal 4
./publisher_tcp 127.0.0.1 PartidoA

# Terminal 5
./publisher_tcp 127.0.0.1 PartidoB
```

Cada publicador enviará automáticamente **10 mensajes** con eventos deportivos simulados (goles, tarjetas, cambios), con una pausa de 1 segundo entre cada uno. Los mensajes se reenviarán a todos los suscriptores conectados y se podrá verificar que llegan completos, en orden y sin pérdidas.

### Verificación del comportamiento TCP

Al finalizar la ejecución, las terminales de los suscriptores deben mostrar:
- Los 20 mensajes totales (10 de PartidoA + 10 de PartidoB) con su timestamp local.
- Un contador de mensajes recibidos incremental sin saltos, confirmando que no hubo pérdida.
- Los mensajes en el mismo orden en que fueron enviados por cada publicador.


## Documentación y Justificación de Librerías

En estricto cumplimiento con los lineamientos del laboratorio, no se utilizaron librerías externas de terceros. El sistema fue construido íntegramente utilizando la interfaz estándar de Berkeley Sockets de los sistemas POSIX.

A continuación se detalla la interacción del código con cada función utilizada:

### 1. Funciones de red (`<sys/socket.h>`)

**`socket(AF_INET, SOCK_STREAM, 0)`:** Utilizada en los tres componentes. Solicita al kernel la creación de un endpoint de comunicación. Se especifica `AF_INET` para la familia IPv4 y `SOCK_STREAM` para forzar el uso de TCP, que ofrece un canal de bytes confiable, ordenado y con control de flujo. El kernel reserva internamente un PCB (Protocol Control Block) para gestionar el estado de la conexión (SYN_SENT, ESTABLISHED, TIME_WAIT, etc.).

**`setsockopt(SO_REUSEADDR)`:** Exclusiva del Broker. Permite reutilizar el puerto inmediatamente después de cerrar el proceso, evitando el error `"Address already in use"` durante el período TIME_WAIT de TCP (aproximadamente 60 segundos), que ocurre tras el cierre de conexiones anteriores.

**`bind(...)`:** Exclusiva del Broker. Asocia los sockets creados con los puertos estáticos 5001 y 5002. Se usa `INADDR_ANY` para aceptar conexiones en cualquier interfaz de red, y `htons()` para convertir los números de puerto a Network Byte Order (big-endian).

**`listen(fd, backlog)`:** Exclusiva del Broker. Marca los sockets como pasivos y dispuestos a aceptar conexiones. El parámetro `backlog` (10) define el tamaño de la cola de conexiones con el Three-Way Handshake completado pero aún no aceptadas con `accept()`. A partir de esta llamada, el kernel responde automáticamente a los SYN entrantes.

**`accept(...)`:** Exclusiva del Broker. Extrae la primera conexión de la cola de backlog y devuelve un nuevo file-descriptor específico para esa sesión. En este punto el Three-Way Handshake ya fue completado por el kernel: el cliente envió SYN, el servidor respondió SYN-ACK, y el cliente confirmó con ACK.

**`connect(...)`:** Utilizada por Publicadores y Suscriptores. Inicia el Three-Way Handshake hacia el Broker. La llamada bloquea hasta que la conexión queda en estado ESTABLISHED; si el servidor no está disponible, retorna inmediatamente con error.

**`send(...)`:** Utilizada por Publicadores y Broker. Copia los bytes al buffer de envío del kernel; TCP se encarga de segmentarlos, asignarles número de secuencia y retransmitirlos si no se recibe ACK en el tiempo esperado.

**`recv(...)`:** Utilizada por el Broker y los Suscriptores. Bloquea hasta que hay bytes disponibles en el buffer de recepción del kernel. TCP garantiza que los bytes se entregan en el mismo orden en que fueron enviados. Un retorno de 0 indica que el peer cerró la conexión (FIN recibido).

**`select(...)`:** Exclusiva del Broker. Permite supervisar múltiples file-descriptors simultáneamente sin crear hilos adicionales. El Broker la usa para monitorear los dos sockets de escucha y todas las conexiones activas de publicadores y suscriptores en un único bucle, retornando control cuando al menos uno está listo para leer o aceptar.

**`close(...)`:** Utilizada por todos los componentes. Inicia el cierre ordenado de TCP enviando un segmento FIN al peer (Four-Way Handshake: FIN → ACK → FIN → ACK). A diferencia de UDP, este cierre genera tráfico de red y garantiza que todos los datos pendientes se entreguen antes de liberar la conexión.

### 2. Transformaciones de red (`<arpa/inet.h>`)

**`htons(...)`** *(Host TO Network Short)*: Convierte el número de puerto del formato little-endian del procesador local al formato big-endian exigido por el estándar de red (RFC 791).

**`inet_pton(...)`:** Convierte la dirección IP en formato de cadena de texto (ej. `"127.0.0.1"`) a su representación binaria de 32 bits en Network Byte Order. Utilizada por Publicadores y Suscriptores al configurar la dirección del Broker antes de llamar a `connect()`.

**`inet_ntoa(...)`** y **`ntohs(...)`:** Utilizadas por el Broker para el proceso inverso. Permiten traducir los datos binarios de los clientes conectados a texto legible para imprimirlos en los logs de consola al aceptar nuevas conexiones.

**`INADDR_ANY`:** Macro utilizada en el Broker para enlazar los sockets a todas las interfaces de red disponibles, no solo a la interfaz loopback.

### 3. Librerías estándar de C (`<string.h>`, `<unistd.h>`, `<time.h>`)

**`memset(...)`:** Inicializa a cero las estructuras `sockaddr_in` y los buffers de recepción, previniendo el envío o lectura de valores residuales de memoria que podrían causar comportamiento indefinido en las funciones de red.

**`snprintf(...)`:** Utilizada en el Publicador para formatear los mensajes con el identificador de partido antes de enviarlos, garantizando que no se supere el tamaño del buffer (`BUF_SIZE`).

**`sleep(...)`:** Utilizada en el Publicador para introducir una pausa de 1 segundo entre mensajes, haciendo visible en Wireshark el flujo de segmentos TCP individuales con sus ACKs correspondientes.

**`time(...)` y `localtime(...)`:** Utilizadas en el Suscriptor para mostrar el timestamp local de cada mensaje recibido, permitiendo verificar empíricamente el orden de entrega y la latencia de la conexión TCP.

**`close(...)`:** Llamada al sistema POSIX que, en TCP, no solo libera el descriptor de archivo local sino que también inicia el proceso de cierre de la conexión enviando un segmento FIN a través de la red.