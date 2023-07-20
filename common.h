#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

void error_handler(int condition, const char *message) {
    if (condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

void warning_time(int condition, const char *message) {
    if (condition) {
        fprintf(stderr, "%s\n", message);
    }
}


int recv_all(int sockfd, void *buffer, int buffer_size) {
    int result = recv(sockfd, buffer, buffer_size, 0);
    if (result == -1 || result == 0) {
        return result;
    }
    // se scad acei bytes cititi pana acum;
    int bytes_to_read = buffer_size - result;
    int cursor = result;
    int read_now;
    while (bytes_to_read > 0) 
    {
        read_now = recv(sockfd, buffer + cursor, bytes_to_read, 0);
        if (read_now == -1 || read_now == 0) {
            return read_now;
        }
        bytes_to_read = bytes_to_read - read_now;
        cursor = cursor + read_now;
    }
    return buffer_size;
}

int send_all(int sockfd, void *buffer, int bytes_to_send) {

    int rc = send(sockfd, buffer, bytes_to_send, 0);
    if (rc == -1) {
        error_handler(1, "common.h send_all, eroare la trimitere\n");
    }
    bytes_to_send = bytes_to_send - rc;
    int current_read;
    while (bytes_to_send > 0)
    {
        current_read = send(sockfd, buffer + rc, bytes_to_send, 0);
        if (current_read == -1) {
            error_handler(1, "common.h send_all, eroare la trimitere in while\n");
        }
        rc = rc + current_read;
        bytes_to_send = bytes_to_send - current_read;
    }
    return rc;
}

int generate_socket_udp(char *port, int sock_type)
{
    struct addrinfo hints;
    struct addrinfo *res;
    int status;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = sock_type;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    if ((sockfd = socket(PF_INET,
                         res->ai_socktype, 0)) < 0)
    {
        fprintf(stderr, "socket creation failed\n");
        exit(1);
    }

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
    {
        perror("setsockopt");
        exit(1);
    }
    int rc = bind(sockfd, (struct sockaddr *)res->ai_addr, sizeof(struct sockaddr));
    if (rc == -1)
    {
        fprintf(stderr, "binding failed\n");
        exit(1);
    }

    free(res);
    return sockfd;
}
