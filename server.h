#include "hashmap.h"
#include "fcntl.h"

struct server
{
    int capacity;
    struct pollfd fd_vector[1024];
    struct subscriber_node *sub_array;
    struct hashmap *pref_map;
    struct subscriber_node *history;
};

struct subscriber_node *find_subscriber(struct subscriber_node *sub_list,
                                        struct subscriber *sub)
{
    if (sub_list == NULL)
    {
        return NULL;
    }
    if (strncmp(sub_list->sub->id, sub->id, strlen(sub->id)) == 0)
    {
        return sub_list;
    }
    return find_subscriber(sub_list->next, sub);
}

struct subscriber_node *find_sub_socked_based(struct subscriber_node *sub_list,
                                              int sockfd)
{
    if (sub_list == NULL)
    {
        return NULL;
    }
    if (sub_list->sub->socket == sockfd)
    {
        return sub_list;
    }
    return find_sub_socked_based(sub_list->next, sockfd);
}

void display_subscriber(struct subscriber *sub)
{
    printf("New client %s connected from %s:%hu\n",
           sub->id, sub->ip_addr, sub->port);
}

void unpack_subscriber_info(struct subscriber *sub, struct registration_protocol *reg_prot)
{
    memset(sub->id, 0, 11);
    memcpy(sub->id, reg_prot->client_id, 10);
    memset(sub->ip_addr, 0, 16);
    memcpy(sub->ip_addr, reg_prot->ipv4_addr, 16);
    memcpy(&sub->port, &reg_prot->port, 2);
}

void load_to_poll(struct server *s, int socket)
{
    s->fd_vector[socket].fd = socket;
    s->fd_vector[socket].events = POLLIN;
}

void unload_to_poll(struct server *s, int socket)
{
    s->fd_vector[socket].fd = -1;
    s->fd_vector[socket].events = -1;
}

void append_sub_subList(struct subscriber_node **sub_list,
                        struct subscriber_node *sub)
{
    if ((*sub_list) == NULL)
    {
        (*sub_list) = sub;
        return;
    }
    struct subscriber_node *temp = (*sub_list);
    while (temp->next != NULL)
    {
        temp = temp->next;
    }
    temp->next = sub;
}

void delete_sub_subList(struct subscriber_node **sub_list,
                        struct subscriber *sub)
{
    struct subscriber_node *temp = (*sub_list);
    struct subscriber_node *prev = NULL;
    if ((*sub_list) == NULL)
    {
        return;
    }
    if ((*sub_list)->next == NULL)
    {
        if (strncmp((*sub_list)->sub->id, sub->id, strlen(sub->id)) == 0) {
            free((*sub_list));
            (*sub_list) = NULL;
            return;
        }
    }
    while (temp != NULL)
    {
        if (strncmp(temp->sub->id, sub->id, strlen(sub->id)) == 0)
        {
            if (temp == (*sub_list))
            {
                (*sub_list) = (*sub_list)->next;
                free(temp);
                return;
            }
            else if (temp->next == NULL)
            {
                prev->next = NULL;
                free(temp);
                return;
            }
            else
            {
                prev->next = temp->next;
                free(temp);
                return;
            }
        }
        prev = temp;
        temp = temp->next;
    }
}

void send_shut_down_subscriber(struct subscriber *sub)
{
    struct my_protocol protocol;
    memset(&protocol, 0, sizeof(struct my_protocol));
    protocol.type = 4;
    char buffer[sizeof(struct my_protocol) + 5];
    protocol.size = 5 + sizeof(struct my_protocol);
    memcpy(buffer, &protocol, sizeof(struct my_protocol));
    memcpy(buffer + sizeof(struct my_protocol), "exit", 5);
    send_all(sub->socket, buffer, sizeof(struct my_protocol) + 5);
}

void free_subscriber_list(struct subscriber_node *list)
{
    if (list == NULL)
    {
        return;
    }
    free_subscriber_list(list->next);
    close(list->sub->socket);
    free_subscriber(list->sub);
    free(list);
}

struct subscriber_node *eliminate_subscriber_from_server(struct server *s,
                                                         int sub_socket)
{
    struct subscriber_node *temp = s->sub_array;
    struct subscriber_node *prev = NULL;
    if (s->sub_array == NULL)
    {
        return NULL;
    }
    if (s->sub_array->next == NULL)
    {
        s->sub_array = NULL;
        return temp;
    }
    while (temp != NULL)
    {
        if (temp->sub->socket == sub_socket)
        {
            if (temp == s->history)
            {
                s->history = temp->next;
                return temp;
            }
            else if (temp->next == NULL)
            {
                prev->next = NULL;
                return temp;
            }
            else
            {
                prev->next = temp->next;
                return temp;
            }
        }
        prev = temp;
        temp = temp->next;
    }
    return NULL;
}

void move_subscriber_to_history(struct server *s,
                                struct subscriber_node *sub_node)
{
    close(sub_node->sub->socket);
    unload_to_poll(s, sub_node->sub->socket);
    append_sub_subList(&s->history, sub_node);
}

struct subscriber *get_new_subscriber(int sockfd_tcp,
                                      struct sockaddr_in serv_addr)
{
    struct sockaddr_in client_addr1;
    socklen_t clen1 = sizeof(client_addr1);
    int new_fd = accept(sockfd_tcp, (struct sockaddr *)&client_addr1, &clen1);
    error_handler(new_fd == -1, "nu a mers acceptarea clientului TCP\n");
    struct subscriber *sub = malloc(sizeof(struct subscriber));
    sub->socket = new_fd;
    sub->port = htons(client_addr1.sin_port);
    char client_buffer[30];
    int result = recv_all(sub->socket, client_buffer, REGISTER_SIZE);
    struct registration_protocol *reg_prot = (struct registration_protocol *)client_buffer;
    unpack_subscriber_info(sub, reg_prot);
    return sub;
}

struct subscriber_node *get_subscriber_from_history(struct server *s,
                                                    struct subscriber *sub)
{
    struct subscriber_node *temp = s->history;
    struct subscriber_node *prev = NULL;
    while (temp != NULL)
    {
        if (strncmp(temp->sub->ip_addr, sub->ip_addr,
                    strlen(sub->ip_addr)) == 0)
        {
            if (temp == s->history)
            {
                s->history = temp->next;
                return temp;
            }
            else if (temp->next == NULL)
            {
                prev->next = NULL;
                return temp;
            }
            else
            {
                prev->next = temp->next;
                return temp;
            }
        }
        prev = temp;
        temp = temp->next;
    }
    return NULL;
}

void restore_previous_subscriber(struct server *s,
                                 struct subscriber_node *sub_node, int new_socket)
{
    sub_node->sub->socket = new_socket;
    load_to_poll(s, new_socket);
    append_sub_subList(&s->sub_array, sub_node);
    // aici de scos efectiv elementul din history
    if (s->history->next == NULL)
    {
        s->history = NULL;
        return;
    }
    struct subscriber_node *temp = s->history;
    while (temp->next != sub_node)
    {
        temp = temp->next;
    }
    temp->next = sub_node->next;
}

void send_all_promisies(struct server *s, struct subscriber_node *sub_node)
{
    for (int i = 0; i < s->capacity; i++)
    {
        if (s->pref_map->array[i] != NULL)
        {
            struct topic_node *temp = s->pref_map->array[i];
            while (temp != NULL)
            {
                if (temp->list_SF_1 != NULL)
                {
                    struct subscriber_node *searched_sub;
                    searched_sub = find_subscriber(temp->list_SF_1,
                                                   sub_node->sub);
                    if (searched_sub != NULL)
                    {
                        send_promiss_from_topic_for_one_sub(temp, sub_node);
                    }
                }
                temp = temp->next;
            }
        }
    }
}