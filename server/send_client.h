#ifndef SEND_CLIENT_H
#define SEND_CLIENT_H
#include <netinet/in.h>   // For sockaddr_in
#include <arpa/inet.h>    // For inet_ntop, inet_pton
#include "glthread.h"
#include <pthread.h>
#include "diff.h"
#include "file_ops.h"
#include "types.h"


typedef struct client_node {
    struct sockaddr_in client_addr;  // Client address structure (stores IP and port)
    glthread_t glthread;// Linked list node
} client_node;

typedef struct client_list {
    glthread_t head;                 // Head of the glthread list for client_node nodes
    pthread_mutex_t mutex;           // Mutex for thread-safe operations on the list
} client_list;

// Arguments structure for sender thread
typedef struct {
    struct sockaddr_in client_addr;
    char *serialized_data;
    size_t data_size;
} sender_thread_args_t;

extern client_list cl_list;
extern glthread_t file_list;

GLTHREAD_TO_STRUCT(thread_cl_node, client_node ,glthread);

void add_client(client_list *cl_list, const struct sockaddr_in *client_addr);
void sync_function();
char* serialize_glthread(glthread_t *glthread_head, int *message_size);
void deserialize_glthread(const char *serialized_data, int message_size, glthread_t *glthread_head);
int get_client_count(client_list *cl_list);
void print_clients(client_list *cl_list);

#endif