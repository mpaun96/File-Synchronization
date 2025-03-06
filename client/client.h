#ifndef CLIENT_H
#define CLIENT_H

#include "glthread.h"
#include <arpa/inet.h>
#include "diff.h"
#include "types.h"
#include <stdatomic.h>  // Include the atomic operations header
#include <stdbool.h>     // For using bool type in C
#include <stddef.h>

GLTHREAD_TO_STRUCT(thread_TLV, TLV, glthread);
extern atomic_bool is_listener_writing;
// Declare global synchronization variables as extern
extern pthread_mutex_t sync_mutex;
extern pthread_cond_t sync_cond;
extern bool sync_complete;

//Monitor file stuff
#define MAX 50
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
#define offset(struct_name, fld_name) \
    (size_t)&(((struct_name *)0)->fld_name)

int keycmp(void* T1, void* T2);
void parseFile(const char *filename, glthread_t *glthread_head);
void monitor_file(const char* filename);


//Send updates stuff
#define BUFFER_SIZE 1024
#define CONNECTION_TIMEOUT_SEC 5
#define RECV_TIMEOUT_SEC 5
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 50001


typedef struct {
    struct sockaddr_in server_addr;
    DiffBuffer diff;
} thread_args_t;

extern int ignore_modifications;

void* tcp_client_thread(void *args);
void error_exit(const char *msg);
void* listener_thread(void *args);

//Receive updates stuff
#define MY_LISTEN_IP "127.0.0.1"
#define LISTEN_PORT 50069

#define BACKLOG 2

void* sync_thread(void *args);
void* tcp_server_thread(void* args);

//other stuff
void write_glthread_to_file(const char *filename, glthread_t *glthread_head);
char* serialize_glthread(glthread_t *glthread_head, int *message_size);
void deserialize_glthread(const char *serialized_data, int message_size, glthread_t *glthread_head);

#endif