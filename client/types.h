#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>  // Include the atomic operations header
#include <stdbool.h>  

extern atomic_bool is_listener_writing;

typedef enum {
    MESSAGE_TYPE_DATA = 1,
    MESSAGE_TYPE_SYNC,
    MESSAGE_TYPE_ACK,
    // Add other message types as needed
} MessageType;

//void append_type(char *serialized_data, unsigned char type, int *size);
//void strip_type(char **serialized_data, unsigned char *type, int *remaining_size);
void append_type(char **serialized_data, unsigned char type, int *size);
void strip_type(char **serialized_data, unsigned char *type, int *remaining_size);
char* serialize_sockaddr_in(struct sockaddr_in *input, int *size);
struct sockaddr_in deserialize_sockaddr_in(char *serialized_data);
void* send_ack(void* arg);


#endif