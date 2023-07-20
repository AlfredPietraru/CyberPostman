#include <stdio.h>     
#include <stdlib.h>     
#include <unistd.h>     
#include <string.h>     
#include <sys/types.h>     
#include <sys/socket.h>     
#include <arpa/inet.h>     
#include <netinet/in.h>
#include <netinet/tcp.h>

#define EXIT 1
#define SUBSCRIBE 2
#define UNSUBCRIBE 3
#define COMMAND_SIZE 53
#define REGISTER_SIZE 28

struct udp_msg_hdr {
    char topic[50];
    uint8_t type;
}__attribute__((packed));

struct registration_protocol {
    char client_id[11];
    char ipv4_addr[16];
    uint16_t port;
}__attribute__((packed));

struct command_protocol {
    char command_type;
    char topic[51];
    char argument;
}__attribute__((packed));

struct my_protocol {
    uint8_t ip_vector[4];
    uint16_t port;
    uint16_t size;
    char topic[50];
    uint8_t type;
}__attribute__((packed));


