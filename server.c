#include "common.h"
#include <poll.h>
#include <sys/select.h>
#include "server.h"
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <math.h>

#define SERVER_CAPACITY 32
// de reparat modul in care se trimit mesajele --> trebuie sa aiba formatul bun
// de schimbat de pe subscrbie topic 0 --> subscribe topic 1 si invers

// python3 udp_client.py --mode manual 127.0.0.1 12345
// python3 udp_client.py 127.0.0.1 12345


//valgrind --leak-check=full --show-leak-kinds=all ./server 12345
#define MAX_CLIENTS 65535
#define MSG_SIZE 1564 


void unsubscribe_command(struct command_protocol *command, int socket, 
    struct server* s) {
    struct subscriber_node *sub_node = find_sub_socked_based(s->sub_array,
             socket);
    struct topic_node *topic = search_topic(s->pref_map, command->topic);
    if (topic != NULL) {
        delete_sub_subList(&topic->list_SF_0, sub_node->sub);
        delete_sub_subList(&topic->list_SF_1, sub_node->sub);
    }
}

void subscribe_command(struct command_protocol *command, int socket, 
    struct server* s) {
        struct subscriber_node *sub_node = find_sub_socked_based(s->sub_array,
             socket); 
        struct subscriber_node *copy_node = create_sub_node(sub_node->sub);
        struct topic_node *topic = search_topic(s->pref_map, command->topic);
        if (topic == NULL) {
            topic = create_topic_node(command->topic, copy_node, command->argument);
            add_to_hashmap(s->pref_map, topic);
            return;
        } else {
            add_subscriber_to_topic(&topic, copy_node, command->argument);
        }
}

void initiliase_server(struct server *s, int sockfd_udp, int sockfd_tcp) {
    s->capacity = SERVER_CAPACITY;
    for(int i =0; i < SERVER_CAPACITY; i++) {
        s->fd_vector[i].fd = -1;
        s->fd_vector[i].events = -1;
        s->fd_vector[i].revents = 0;
    }
    s->fd_vector[0].fd = 0;
    s->fd_vector[0].events = POLLIN;

    s->fd_vector[1].fd = sockfd_udp;
    s->fd_vector[1].events = POLLIN;

    s->fd_vector[2].fd = sockfd_tcp;
    s->fd_vector[2].events = POLLIN;

    s->sub_array = NULL;
    s->pref_map = create_hashmap(SERVER_CAPACITY);
    s->history = NULL;
}


void free_server(struct server *s) {
    free_subscriber_list(s->history);
    free_subscriber_list(s->sub_array);
    free_map(s->pref_map);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    error_handler(argc < 2, "trebuie date mai multe argumente\n");
    error_handler(argc > 2, "ai dat prea multe argumente\n");

    int sockfd_udp = generate_socket_udp(argv[1], SOCK_DGRAM);

    struct sockaddr_in serv_addr; 
    int sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    error_handler(sockfd_tcp == -1, "nu a mers sa creeze socketul\n");

    int no_delay_flag = 1;
    setsockopt(sockfd_tcp, IPPROTO_TCP, TCP_NODELAY, & no_delay_flag,
        sizeof(no_delay_flag));

    int enable = 1;
    int result = setsockopt(sockfd_tcp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    error_handler(result < 0, "setsockopt(SO_REUSEADDR) failed");

    enable = 1;
    result = setsockopt(sockfd_udp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    error_handler(result < 0, "setsockopt(SO_REUSEADDR) failed");

    socklen_t socket_len = sizeof(struct sockaddr_in);
    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;

    uint16_t int_port;
    int rc = sscanf(argv[1], "%hu", &int_port);
    error_handler(rc != 1, "given port is invalid\n");
    serv_addr.sin_port = htons(int_port);
    rc = inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr.s_addr);
    error_handler(rc < 0, "nu a mers inet_pton\n");
    
    rc = bind(sockfd_tcp, (const struct sockaddr*)&serv_addr, 
        sizeof(serv_addr));
    error_handler(rc == -1, "binding does not work\n");

    int status = listen(sockfd_tcp, 32);
    error_handler(status == -1, "nu merge sa asculte server-ul\n");

    struct server s;
    initiliase_server(&s, sockfd_udp, sockfd_tcp);

    unsigned char buffer[MSG_SIZE];
    int exit_value = 0;
    char command_buf[100];
    while(1) {
        int poll_count = poll(s.fd_vector, s.capacity, -1);
        error_handler(poll_count == -1, "nu merge poll cum trebuie\n");
        
        for(int i = 0; i < s.capacity; i++) {
            // Check if someone's ready to read
            if (s.fd_vector[i].revents & POLLIN) {

                if (s.fd_vector[i].fd == 0) {
                    // user input, respond only at exit
                    char buffer[10];
                    memset(buffer, 0, 10);
                    fgets(buffer, 9, stdin);
                    if (strncmp(buffer, "exit", 4) != 0) {
                        printf("There is no such command. %s\n", buffer);
                    } else {
                        free_server(&s);
                        //inchid fiecare socket
                        for(int i = 0; i < s.capacity; i++) {
                            if (s.fd_vector[i].fd != -1) {
                                close(s.fd_vector[i].fd);
                            }
                        }
                        return 0;
                    }
                } else if (s.fd_vector[i].fd == sockfd_tcp) {

                    struct subscriber *new_sub = get_new_subscriber(sockfd_tcp,
                      serv_addr);
                    if (find_subscriber(s.sub_array, new_sub) != NULL) {
                        fprintf(stdout, "Client %s already connected.\n", new_sub->id);
                        close(new_sub->socket);
                        free_subscriber(new_sub);
                    } else {
                        display_subscriber(new_sub);
                        //subscriber in history, get him back on server;
                        struct subscriber_node* old_sub = find_subscriber(s.history, new_sub);
                        if (old_sub != NULL) {
                            //the subscriber is actually on history, get him out of there
                            restore_previous_subscriber(&s, old_sub, new_sub->socket);
                            send_all_promisies(&s, old_sub);
                            free_subscriber(new_sub);
                        } else {
                            append_sub_subList(&s.sub_array, create_sub_node(new_sub));
                            load_to_poll(&s, new_sub->socket);
                        }
                    } 
                    break;
                } else if (s.fd_vector[i].fd == sockfd_udp) {

                    struct sockaddr_in from;
                    socklen_t fromlen = sizeof(from);
                    // first 8 bytes for struct my_protocol general info
                    uint16_t rc = recvfrom(sockfd_udp, buffer + 8, 
                            MSG_SIZE - 1, 0, (struct sockaddr*)&from, &fromlen);
                    error_handler(rc == -1, "nu merge sa trimita pachete\n");
                    
                    //create struct my_protocol header
                    memcpy(buffer, &from.sin_addr.s_addr, 4);
                    uint16_t reverse_port = htons(from.sin_port);
                    memcpy(buffer + 4, &reverse_port, 2);
                    uint16_t new_rc = rc + 9;
                    memcpy(buffer + 6, &new_rc, 2);
                    // test.py has an error here if i do not add string terminator
                    buffer[rc + 8] = '\0';
                    struct udp_msg_hdr *msg_hdr = (struct udp_msg_hdr*)(buffer + 8);
                    struct topic_node *required_topic = 
                         search_topic(s.pref_map, msg_hdr->topic);
                  
                      if (required_topic != NULL) {
                        send_to_all_clients(required_topic, buffer, rc + 9);
                      } else {
                        continue;
                    } 
                    error_handler(rc == -1, "nu vin pachete udp\n");
                    break;

                } else {
                    //just a regular client, receive his command and execute it
                        memset(command_buf, 0, 100);
                    //create a system for remembering rest of commands 
                        int nbytes = recv_all(s.fd_vector[i].fd, 
                        command_buf, COMMAND_SIZE);
                        struct command_protocol *cmd_prot =  (struct command_protocol*)command_buf;
                        if (cmd_prot->command_type == EXIT) {
                            struct subscriber_node* to_eliminate = 
                                eliminate_subscriber_from_server(&s, i); 
                            printf("Client %s disconnected.\n", to_eliminate->sub->id);
                            move_subscriber_to_history(&s, to_eliminate);
                            continue;
                        } else if (cmd_prot->command_type == UNSUBCRIBE) {
                            unsubscribe_command(cmd_prot, i, &s);
                            continue;
                        } else if (cmd_prot->command_type == SUBSCRIBE) {
                            subscribe_command(cmd_prot, i, &s);
                            continue;
                        } else {
                            break;
                        }
                }
            }    
        } 
    } 
}