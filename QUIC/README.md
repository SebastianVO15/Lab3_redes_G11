# Laboratorio 3 - G11 - Implementacion QUIC
## 1. Objetivo

Construir una version simplificada de QUIC que permita:

- handshake entre cliente y broker
- envio de datos por streams
- ACK y retransmision para mejorar confiabilidad
- suscripcion por topico
- deteccion de paquetes fuera de orden

## 2. Estructura

- quicSL.h: tipos de paquete, header y funciones comunes (build, send, recv, conn_id)
- broker_quic.c: broker central; registra subscribers, reenvia mensajes y responde ACK
- publisher_quic.c: publica eventos por topico y stream con retransmision
- subscriber_quic.c: se suscribe a un topico y recibe eventos

## 3. Requisitos

- GCC
- Se necesita WSL para compilar el lab

## 4. Compilacion

Desde la carpeta `QUIC`:

```bash
gcc -o broker_quic broker_quic.c -lpthread
gcc -o publisher_quic publisher_quic.c
gcc -o subscriber_quic subscriber_quic.c
```

## 5. Ejecucion rapida

Abrir 5 terminales en QUIC y ejecutar en este orden:

1. Broker:

./broker_quic

2. Subscriber para PartidoA:

./subscriber_quic "PartidoA"

3. Subscriber para PartidoB:

./subscriber_quic "PartidoB"

4. Publisher de PartidoA, stream 1:

./publisher_quic "PartidoA" 1

5. Publisher de PartidoB, stream 2:

./publisher_quic "PartidoB" 2

## 6. Uso de programas

```bash
./publisher_quic <topic> <stream_id>
./subscriber_quic <topic>
```

Ejemplo:

```bash
./publisher_quic "ClasicoBogota" 3
./subscriber_quic "ClasicoBogota"
```

## 7. Protocolo implementado

### Header

se usa la estructura de QuicHeader que ocupa 13 bytes de esta manera:

- type (1B): tipo de paquete (INITIAL, HANDSHAKE, STREAM, ACK, SUBSCRIBE o CLOSE).
- conn_id (4B): identificador de la conexion para distinguir clientes.
- stream_id (2B): identificador del stream del publisher.
- seq (4B): numero de secuencia del mensaje para controlar orden y ACK.
- payload_len (2B): tamaño del payload en bytes.

### Tipos de paquete

- PKT_INITIAL (0x01): iniciar conexion
- PKT_HANDSHAKE (0x02): respuesta de handshake
- PKT_STREAM (0x03): datos
- PKT_ACK (0x04): confirmacion
- PKT_SUBSCRIBE (0x05): suscripcion a topico
- PKT_CLOSE (0x06): cierre de conexion

### Flujo general

1. Cliente envia PKT_INITIAL
2. Broker responde PKT_HANDSHAKE
3. Subscriber envia PKT_SUBSCRIBE
4. Publisher envia PKT_STREAM con formato topic|mensaje
5. Broker envia PKT_ACK al publisher y reenvia a subscribers del topico
6. Al final se envia PKT_CLOSE

## 8. Confiabilidad y limites

Implementado:

- ACK por mensaje
- timeout de 500 ms en publisher
- hasta 5 reintentos (MAX_RETRIES)
- numero de secuencia por stream
- advertencia de desorden en subscriber

No implementado (fuera del alcance del lab):

## 9. Parametros editables

- Puerto del broker: BROKER_PORT en quic.h
- Timeout y reintentos: TIMEOUT_MS y MAX_RETRIES en publisher_quic.c
- Maximo de subscribers: MAX_SUBS en broker_quic.c


## 10. Implementacion en codigo

### quic.h — Funciones comunes del protocolo

Todas las funciones estan declaradas como static inline para que el compilador las
expanda en el sitio de llamada sin overhead de funcion.

#### build_packet
Construye un paquete QUIC listo para enviar. Llena cada campo del header y copia el
payload al buffer. Retorna el tamaño total del paquete en bytes.

Internamente convierte conn_id, stream_id, seq y payload_len a network byte order
antes de escribirlos, usando htonl() para los campos de 4 bytes y htons() para
los de 2 bytes.

#### decode_header
Convierte todos los campos del header de network byte order a host byte order. Se
llama automaticamente dentro de recv_packet cada vez que se recibe un paquete.

#### send_packet
Envia un paquete ya construido a una direccion destino usando sendto() de UDP.


#### recv_packet
Recibe un datagrama UDP con recvfrom(), verifica que tenga al menos 13 bytes (tamaño
minimo de un header valido), llama a decode_header y guarda la direccion del remitente.
Retorna -1 si el paquete es demasiado corto.

#### new_conn_id
Genera un ID de conexion aleatorio combinando el tiempo actual lo que evita colisiones cuando dos procesos arrancan en el mismosegundo.

#### udp_socket_bind
Crea un socket UDP (AF_INET, SOCK_DGRAM) y lo liga al puerto indicado con bind().
Retorna el file descriptor del socket.


### broker_quic.c — Implementacion del broker

#### Estado interno


typedef struct {
    int                active;    // 1 = conectado, 0 = slot libre
    uint32_t           conn_id;   // ID de este subscriber
    char               topic[64]; // topico al que esta suscrito
    struct sockaddr_in addr;      // direccion IP:puerto para enviarle mensajes
} Subscriber;

static Subscriber      subs[MAX_SUBS];
static int             sub_count = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


El pthread_mutex_t protege el arreglo subs[] de condiciones de carrera. Sin el mutex,
si dos paquetes llegaran al mismo tiempo podrian escribir en la misma posicion del
arreglo simultaneamente y corromper los datos.

#### register_subscriber

Busca el primer slot con active = 0 en el arreglo y lo ocupa con los datos del nuevo
subscriber. Usa pthread_mutex_lock y pthread_mutex_unlock para que la busqueda y
escritura sean atomicas.


#### broadcast

Itera linealmente sobre subs[] y para cada subscriber con active = 1 cuyo topic
coincida con el del mensaje, construye un PKT_STREAM con el conn_id del subscriber y lo envia con send_packet. Usa mutex para leer el arreglo de
forma segura.

#### send_ack

Construye un PKT_ACK cuyo campo seq coincide con el del paquete recibido, y lo
envia de vuelta al remitente. El publisher verifica ese seq para confirmar que el ACK
corresponde al mensaje que envio.


#### Loop principal

Corre en un while(1) llamando a recv_packet. Segun el type del paquete recibido:

- PKT_INITIAL: responde PKT_HANDSHAKE con el mismo conn_id
- PKT_SUBSCRIBE: llama a register_subscriber + send_ack
- PKT_STREAM: separa topic y mensaje por "|", llama a send_ack y luego a broadcast
- PKT_CLOSE: marca subs[i].active = 0 para liberar el slot


### publisher_quic.c — Implementacion del publisher

#### Handshake

Genera un conn_id con new_conn_id(), construye un PKT_INITIAL con build_packet
y lo envia al broker. Luego llama a recv_packet esperando un PKT_HANDSHAKE. Si el
tipo recibido no es PKT_HANDSHAKE, imprime error y termina.

#### send_reliable

Es la funcion central de la confiabilidad. Implementa Stop-and-Wait ARQ: envia un paquete
y no avanza hasta recibir el ACK correspondiente.


Pasos internos:

1. Configura un timeout de TIMEOUT_MS milisegundos en el socket con setsockopt usando SO_RCVTIMEO. Cuando el timeout expira, recv_packet retorna con error enlugar de bloquear indefinidamente.
2. Llama a send_packet para enviar el paquete.
3. Llama a recv_packet esperando el ACK.
4. Verifica que ack.hdr.type == PKT_ACK y que ack.hdr.seq == expected_seq. El chequeo del seq evita que un ACK retrasado de un mensaje anterior se confunda con el ACK del mensaje actual.
5. Si el ACK no llega o es incorrecto, reintenta desde el paso 2 hasta MAX_RETRIES veces.
6. Si se agotan los reintentos, retorna -1 (no se pudo).

#### Formato del payload PKT_STREAM

"PartidoA|Gol de Equipo A al minuto 32"
└──────┘ └──────────────────────────┘
  topic           mensaje


El broker usa strchr(payload, '|') para dividir el string y saber a que topico reenviar.
Si el payload no contiene "|", el paquete se descarta.

#### Envio de eventos
Itera sobre el arreglo events[] usando seq = i como numero de secuencia. Al terminar,
envia PKT_CLOSE para notificar al broker.

### subscriber_quic.c — Implementacion del subscriber

#### Puerto efimero

El subscriber llama a bind() con puerto 0. El sistema operativo asigna automaticamente
un puerto libre. Cuando el subscriber hace sendto() al broker, el SO registra la
combinacion IP:puerto del subscriber en el socket, permitiendo que el broker le responda
a esa misma direccion con send_packet.

#### Handshake y suscripcion
Mismo flujo que el publisher para el handshake. Luego construye un PKT_SUBSCRIBE cuyo
payload es el nombre del topico y espera un PKT_ACK de confirmacion. A partir de ese
punto el broker tiene registrada la direccion del subscriber y le reenviara mensajes.

#### Deteccion de desorden
Se espera consatntemente un valor que es el del paquete sigueinte al anterior recibido, si llega un paquete diferente al sigueinte se asume que se perdio el anterior o se retraso.

## 12. Confiabilidad y limites

En este laboratorio se Implemento:
- ACK por mensaje
- timeout de 500 ms en publisher 
- hasta 5 reintentos con advertencia en cada intento fallido
- numero de secuencia por stream para controlar orden
- advertencia de desorden en subscriber cuando seq != last_seq + 1
- cierre limpio con PKT_CLOSE

### Manejo de paquetes grandes
El tamaño maximo de un paquete esta definido por `BUF_SIZE = 1200` bytes en `quic.h`.
Este valor se eligio para evitar la fragmentacion IP: la MTU tipica de Ethernet es
1500 bytes; restando el header IP (20B) y el header UDP (8B) quedan 1472 bytes
disponibles. Usar 1200 bytes deja margen para headers adicionales en redes con MTU
reducida como VPNs o tuneles.

Si un payload supera `MAX_PAYLOAD = BUF_SIZE - HEADER_SIZE` bytes, `memcpy` en
`build_packet` escribiria fuera del buffer. En este laboratorio los mensajes son cortos
y no se alcanza ese limite. Para produccion, la solucion seria truncar o fragmentar
el payload antes de llamar a `build_packet`.

### Manejo de ACKs duplicados o retrasados
send_reliable verifica que ack.hdr.seq == expected_seq. Si llega un ACK con un
numero de secuencia diferente (por ejemplo un ACK retrasado del mensaje anterior), se
descarta y se espera de nuevo hasta agotar el timeout. Esto evita que el publisher
avance incorrectamente ante un ACK que no corresponde al mensaje actual (Como en UDP).

### Paquetes con tipo desconocido
El switch del broker tiene un caso default en el que imprime el tipo recibido en
hexadecimal y descarta el paquete sin afectar el estado del sistema:

### Subscriber que se desconecta sin enviar PKT_CLOSE
Si un subscriber se cae sin enviar PKT_CLOSE, su slot queda con active = 1 en el
arreglo del broker. El proximo broadcast intentara enviarle mensajes y sendto
retornara error silenciosamente (el datagrama UDP simplemente se pierde). No afecta a
los demas subscribers. Una mejora posible es implementar heartbeats periodicos para
detectar subscribers caidos.

## 13. Nota

En esta implementacion se obvio y evito la implementacion de un sistema de cifrado como TSL para practicidad de este laboratorio debido a que el proposito es entender como funciona y no hacerlo del todo completo.
