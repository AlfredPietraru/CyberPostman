#include "stdlib.h"
#include "stdio.h"
#include "udp_msg.h"

struct subscriber {
    int socket;
    char id[11];
    char ip_addr[16];
    uint16_t port;
};

struct subscriber_node {
    struct subscriber *sub;
    struct subscriber_node *next;
};

struct promise {
    int size;
    char *msg;
    struct subscriber *sub;
    struct promise *next;
};

struct topic_node {
    char *name;
    struct subscriber_node *list_SF_1;
    struct subscriber_node *list_SF_0;
    struct promise *promise_list;
    struct topic_node *next;
};

struct hashmap {
    struct topic_node** array;
    int capacity;
};


struct promise *create_promise(char *msg, struct subscriber *sub, int size) {
    struct promise * my_promise = malloc(sizeof(struct promise));
    my_promise->size = size;
    my_promise->msg = msg;
    my_promise->sub = sub;
    my_promise->next = NULL;
    return my_promise; 
}

struct subscriber_node *create_sub_node(struct subscriber *sub) {
    struct subscriber_node* node = malloc(sizeof(struct subscriber_node));
    node->sub = sub;
    node->next = NULL;
    return node; 
}

int send_one_promise(struct promise* promise, 
    struct subscriber_node* sub_node) {
    int rc = -2; 
     if (strncmp(promise->sub->ip_addr, sub_node->sub->ip_addr, 
            strlen(sub_node->sub->ip_addr)) == 0) {
        rc = send(promise->sub->socket, promise->msg, promise->size, 0);
    }
    return rc;
}

void free_all_promises (struct promise *my_promise) {
    if (my_promise->next != NULL) {
        free_all_promises(my_promise->next);
    }
    free(my_promise);
}

void free_subscriber(struct subscriber* sub) {   
    free(sub);
}

struct topic_node *create_topic_node(char *topic_name, 
struct subscriber_node *sub, uint8_t SF) {
    struct topic_node *my_node = malloc(sizeof(struct topic_node));
    my_node->list_SF_0 = NULL;
    my_node->list_SF_1 = NULL;
    if (SF == 0) {
        my_node->list_SF_0 = sub;
    } else {
        my_node->list_SF_1 = sub;
    }
    my_node->promise_list = NULL;
    my_node->name = calloc(strlen(topic_name) + 1, 1);
    memcpy(my_node->name, topic_name, strlen(topic_name));
    my_node->next = NULL;
    return my_node;
}

void add_subscriber_to_topic(struct topic_node **topic, 
    struct subscriber_node *sub, uint8_t SF) {
        if (SF == 0) {
            if ((*topic)->list_SF_0 == NULL) {
                (*topic)->list_SF_0 = sub;
            } else {
                struct subscriber_node *temp = (*topic)->list_SF_0;
                while(temp->next != NULL) {
                    temp = temp->next;
                }
                temp->next = sub;
                return; 
            }
        } else {
             if ((*topic)->list_SF_1 == NULL) {
                (*topic)->list_SF_1 = sub;
            } else {
                struct subscriber_node *temp = (*topic)->list_SF_1;
                while(temp->next != NULL) {
                    temp = temp->next;
                }
                temp->next = sub;
                return; 
            }
        }
    }

void free_topic_node(struct topic_node *node) {
    free(node->name);
    if (node->list_SF_1 != NULL) {
        free(node->list_SF_1);
    }
    if (node->list_SF_0 != NULL) {
        free(node->list_SF_0);
    }
    if (node->promise_list != NULL) {
        free_all_promises(node->promise_list);
    }
    free(node);
}

void free_all_topics_nodes (struct topic_node *node) {
    if (node ->next != NULL) {
        free_all_topics_nodes(node->next);
    }
    free_topic_node(node);
}

unsigned long hash(char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c;

    return hash;
}

struct hashmap *create_hashmap(int capacity) {
    struct hashmap *map = malloc(sizeof(struct hashmap));
    map->capacity = capacity;
    map->array = malloc(sizeof(struct topic_node*) * capacity);
    for(int i =0; i < capacity; i++) {
        map->array[i] = NULL;
    }
    return map; 
}

void add_to_hashmap (struct hashmap* map, struct topic_node *topic) {
    int final_index = hash(topic->name) % map->capacity;
    if (map->array[final_index] == NULL) {
        map->array[final_index] = topic;
    } else {
        struct topic_node* temp = map->array[final_index];
        while(temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = topic;
    }
}

struct topic_node *search_topic(struct hashmap* map, char *name) {
    int index = hash(name) % map->capacity;
    struct topic_node *needed_node = map->array[index];
    while(needed_node != NULL) {
        if (strncmp(needed_node->name, name, 50) == 0) {
            return needed_node;
        } 
        needed_node = needed_node->next;
    }
    return NULL;
}

void free_map(struct hashmap* map) {
    for(int i = 0; i < map->capacity; i++) {
        if (map->array[i] != NULL) {
            printf("%s\n", map->array[i]->name);
            free_all_topics_nodes(map->array[i]);
        }
    }
    free(map->array);
    free(map);
}

void display_hashmap(struct hashmap *map) {
    for(int i =0; i < map->capacity; i++) {
        if (map->array[i] != NULL) {
            printf("%s\n", map->array[i]->name);
        } else {
            printf("NULL\n");
        }
    }
}

void send_to_SF_0_clients(struct topic_node *node, char *header, int size) {
    if (node->list_SF_0 == NULL) {
        return;
    } 
    struct subscriber_node *temp = node->list_SF_0;
    int rc = -1;
    while(temp != NULL) {
        rc = send(temp->sub->socket, header, size, 0);
        temp = temp->next;
    }
}

void add_another_promise(struct topic_node *node, struct promise* my_promise) {
    if (node->promise_list == NULL) {
        node->promise_list = my_promise;
    } else {
        struct promise *temp = node->promise_list;
        while(temp ->next != NULL) {
            temp = temp->next;
        }
        temp->next = my_promise;
    }
}

void send_to_SF_1_clients(struct topic_node *node, char *header, int size) {
    if (node->list_SF_1 == NULL) {
        return;
    } 
    struct subscriber_node *temp = node->list_SF_1;
    int rc = -1;
    while(temp != NULL) {
        rc = send(temp->sub->socket, header, size, 0);
         if (rc == -1) {
            struct promise* my_promise = create_promise(header, temp->sub, size);
            add_another_promise(node, my_promise);
        }
        temp = temp->next;
    }
}

void send_to_all_clients(struct topic_node *node, char *header, int size) {
    if (node->list_SF_0 == NULL && node->list_SF_1 == NULL) {
        return;
    }
    struct subscriber_node* temp;
    if (node->list_SF_0 != NULL && node->list_SF_1 != NULL) {
        send_to_SF_0_clients(node, header, size);
        send_to_SF_1_clients(node, header, size);
    } else if (node->list_SF_0 != NULL && node->list_SF_1 == NULL) {
        send_to_SF_0_clients(node, header, size);
    } else if (node->list_SF_0 == NULL && node->list_SF_1 != NULL) {
       send_to_SF_1_clients(node, header, size);
    }
}

void send_promiss_from_topic_for_one_sub(struct topic_node *node, 
    struct subscriber_node* sub_node) {
        struct promise *temp = node->promise_list;
        int rc;
        if (temp == NULL) {
            return;
        }
        //trimitem tot ceea ce e de trimis
        while(temp != NULL) {
            send_one_promise(temp, sub_node);
            temp = temp->next;
        }
}