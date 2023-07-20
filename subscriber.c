#include "udp_msg.h"
#include "common.h"
#include <poll.h>
#include <math.h>


// valgrind --leak-check=full --show-leak-kinds=all ./subscriber C1 127.0.0.1 12345
#define BUFFER_OFFSET 8
#define MSG_SIZE 1620

void display_general_info(struct my_protocol* protocol) {
    printf("%s - ", protocol->topic);
    if (protocol->type == 0) {
        printf("%s - ", "INT");
    } else if (protocol->type == 1) {
        printf("%s - ", "SHORT_REAL");
    } else if (protocol->type == 2) {
        printf("%s - ", "FLOAT");
    } else if (protocol->type == 3) {
        printf("%s - ", "STRING");
    }
}

void manage_value (struct my_protocol *protocol, char *message) {
    int protocol_size = sizeof(struct my_protocol);
    if(protocol->type == 0) {
        uint8_t sign;
        memcpy(&sign, message + protocol_size, 1);
        int value;
        memcpy(&value, message + protocol_size + 1, 4);
        value = htonl(value);
        if (sign == 0) {
            printf("%d\n", value);
        }  else {
            printf("%d\n", -value);
        }
    } else if (protocol->type == 1) {
        uint16_t value;
        memcpy(&value, message + protocol_size, 2);
        value = htons(value);
        float final_value = (float)value/100;
        printf("%.2f\n", final_value);
    } else if (protocol->type == 2) {
        uint8_t sign;
        memcpy(&sign, message + protocol_size, 1);
        uint32_t value;
        memcpy(&value, message + protocol_size + 1, 4);
        uint8_t move_comma;
        memcpy(&move_comma, message + protocol_size + 5, 1);
        value = htonl(value);
        if (sign == 1) {
            printf("-");
        }
        double reduce = pow(10, -move_comma);
        double final_value = value * reduce;
        printf("%.*f\n", move_comma, final_value);
    } else if (protocol->type == 3) {
        printf("%s\n", message + protocol_size);
    } else {
        warning_time(1, "Nu trebuia sa fie type > 3 sau type < 0");
    }
}

int recv_protocol_message(int sockfd_tcp, char* message, int message_max_size) {
    int rc = recv(sockfd_tcp, message, message_max_size, 0);
    long int protocol_size = sizeof(struct my_protocol);
    int bytes_left_from_protocol = protocol_size - rc;
    int size_till_now = rc;
    while(bytes_left_from_protocol > 0) {
        rc = recv(sockfd_tcp, message + size_till_now, bytes_left_from_protocol, 0);
        if (rc == -1 || rc == 0) {
            break;
        }
        size_till_now = size_till_now + rc;
        bytes_left_from_protocol = bytes_left_from_protocol - rc;
    }
    if (rc == -1 || rc == 0) {
        return rc;
    }
    struct my_protocol *protocol = (struct my_protocol*)message;
    uint16_t size = protocol->size;
    uint16_t content_size = size - 8;
    if (content_size - rc < 0) {
        return rc;
    } else {
        rc = recv_all(sockfd_tcp, message + rc, content_size - rc);
        return rc; 
    }  
}

int read_one_message(int sockfd_tcp, char *message, int max_size_to_read) {
    int rc = recv_all(sockfd_tcp, message, sizeof(struct my_protocol));
    if (rc == 0) {
        return 0;
    }
    if (rc == -1) {
        warning_time(1, "nu a mers de citit buffer-ul in subscriber\n");
        return -1;
    }
    max_size_to_read = max_size_to_read - rc;
    struct my_protocol *protocol = (struct my_protocol *)message;
    if (protocol->type == 4) {
        return 0;
    } 
    //de facut situatia cand mesajul este mai mare decat buffer-ul
    
    rc = recv_all(sockfd_tcp, message + rc, protocol->size - sizeof(struct my_protocol));
    if (rc == -1) {
        warning_time(1, "nu merge de citit continutul mesajului in subscriber\n");
        return -1;
    }
    display_general_info(protocol);
    manage_value(protocol, message);
    return rc;
}

int get_package_buffer(char *message) {
     struct my_protocol *protocol = (struct my_protocol *)message;
        if (protocol->type == 5) {
            return 0;
        }  else {
    }
    return protocol->size;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc < 4)
    {
        fprintf(stderr, "lipsesc mai multe argumente\n");
        exit(1);
    }
    if (argc > 5)
    {
        fprintf(stderr, "nu ar trebui date asa de multe argumente\n");
        exit(1);
    }
    char *client_id = argv[1];
    char *string_ip_addr = argv[2];
    char *string_port = argv[3];

    struct sockaddr_in serv_addr;
    int sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    error_handler(sockfd_tcp == -1, "nu se creaza socket-ul\n");

    int enable = 1;
    int rc = setsockopt(sockfd_tcp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    error_handler(rc < 0, "setsockopt(SO_REUSEADDR) failed\n");

    int no_delay_flag = 1;
     setsockopt(sockfd_tcp, IPPROTO_TCP, TCP_NODELAY, & no_delay_flag,
        sizeof(no_delay_flag));

    socklen_t socket_len = sizeof(struct sockaddr_in);
    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;

    uint16_t int_port;
    rc = sscanf(string_port, "%hu", &int_port);
    error_handler(rc != 1, "given port is invalid\n");

    serv_addr.sin_port = htons(int_port);
    rc = inet_pton(AF_INET, string_ip_addr, &serv_addr.sin_addr.s_addr);
    error_handler(rc < 0, "nu a mers inet_pton\n");

    rc = connect(sockfd_tcp, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    error_handler(rc == -1, "nu merge sa se conecteze\n");

    struct registration_protocol reg_prot;
    memset(&reg_prot, 0, sizeof(struct registration_protocol));

    memcpy(&reg_prot.client_id, client_id, strlen(client_id));
    reg_prot.client_id[strlen(client_id)]= '\0'; 
    memcpy(&reg_prot.ipv4_addr, string_ip_addr, strlen(string_ip_addr));
    reg_prot.client_id[strlen(string_ip_addr)] = '\0';
    uint16_t port = atoi(string_port);
    reg_prot.port = port;
    char register_buffer[30];
    memcpy(register_buffer, &reg_prot, sizeof(struct registration_protocol));
    memcpy(register_buffer + 27, &port, 2);
    int reached_server = send(sockfd_tcp, register_buffer, REGISTER_SIZE, 0);

    error_handler(reached_server == -1, "nu gaseste serverul\n");

    struct pollfd fd_vector[2];
    fd_vector[0].fd = 0;
    fd_vector[0].events = POLLIN;

    fd_vector[1].fd = sockfd_tcp;
    fd_vector[1].events = POLLIN;

    char command_buf[100];
    char message[MSG_SIZE];
    memset(command_buf, 0, 100);
    int breaking_point = 0;
    while (1)
    {
        int poll_count = poll(fd_vector, 2, -1);
        error_handler(poll_count == -1, "nu merge poll cum trebuie\n");
        for (int i = 0; i < 2; i++)
        {
            if (fd_vector[i].revents & POLLIN)
            {
                if (fd_vector[i].fd == 0)
                {
                    fgets(command_buf, 99, stdin);
                    struct command_protocol user_command;
                    memset(&user_command, 0, sizeof(struct command_protocol)); 
                    int buffer_size;
                    if (strstr(command_buf, "exit") != NULL)
                    {   
                        user_command.command_type = EXIT;
                        user_command.argument = -1;
                        rc = send_all(sockfd_tcp, &user_command, COMMAND_SIZE);
                        error_handler(rc == -1, "nu s-a trimis mesajul\n");
                        close(sockfd_tcp);
                        return 0;
                        //breaking_point = 1;
                    }
                    else if (strstr(command_buf, "unsubscribe") != 0)
                    {
                        user_command.command_type = UNSUBCRIBE;
                        memcpy(&user_command.topic, command_buf + strlen("unsubscribe") + 1, 50);
                        //ajungeam sa am /n pe pozitia pe care trebuia sa am '\0', si nu il gasea in hashmap
                        user_command.topic[strlen(user_command.topic) - 1] = '\0';    
                        user_command.argument = -1; 
                        rc = send_all(sockfd_tcp, &user_command, COMMAND_SIZE);
                        error_handler(rc == -1, "nu s-a trimis mesajul\n");
                        printf("Unsubscribed from topic.\n");
                        continue;
                    }
                    else if (strstr(command_buf, "subscribe") != NULL)
                    {
                        strtok(command_buf, " ");
                        char *topic = strtok(NULL, " ");
                        if (topic == NULL) {
                            break;
                        }
                        warning_time(topic == NULL, "nu e valida comanda data la subscribe, in subscriber\n");
                        char *SF = strtok(NULL, " ");
                        warning_time(SF == NULL, "trebuia sa fie argument de SF dat, si nu e\n");
                        if (SF == NULL) {
                            break;
                        }
                        uint8_t int_SF = atoi(SF);
                        warning_time(int_SF != 0 && int_SF != 1, "nu are SF valoarea buna\n");
                        if (int_SF != 0 && int_SF != 1) {
                            break;
                        }
                        int topic_size = SF - topic - 1;
                        warning_time(topic_size > 50, "topic-ul dat nu este valid in subscriber.c\n");
                        
                        user_command.command_type = SUBSCRIBE;
                        memcpy(&user_command.topic, topic, topic_size); 
                        user_command.argument = int_SF;
                        rc = send_all(sockfd_tcp, &user_command, COMMAND_SIZE);
                        error_handler(rc == -1, "nu s-a trimis mesajul\n");
                        printf("Subscribed to topic.\n");
                         continue;
                    }
                    else
                    {
                        break;
                    }
                }

                if (fd_vector[i].fd == sockfd_tcp)
                {
                     rc = read_one_message(sockfd_tcp, message, MSG_SIZE);
                    //am primit bucati din mesaj, trebuie sa ma ocup de ele
                    error_handler(rc == -1, "ar fi trebuit sa primeasca mesajul de la server\n");
                    if (rc == 0) {
                        close(sockfd_tcp);
                        return 0;
                    }
                }
            }
        }
    }
}