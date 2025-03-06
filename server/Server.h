#ifndef SERVER_H
#define SERVER_H


#define PORT 50001
#define MAX_CLIENTS 1000
#define BUFFER_SIZE 4096  // Adjust this based on your serialization function
#define MAX_POLL_EVENTS 2
#include <stdatomic.h>
#include "send_client.h"
#include <poll.h>   // Required for struct pollfd
#include <unistd.h> // For sleep()
#include "types.h"

//extern client_list cl_list;
extern struct pollfd poll_fds[MAX_POLL_EVENTS];

/*
typedef struct {
    int client_socket;
    struct sockaddr_in client_add;
} client_thread_args_t;
*/

typedef struct {
    int client_socket;
    struct sockaddr_in client_add;
    char* serialized_message;
    int size_of_message;
} client_thread_args_t;

void* handle_client(void* args);
void start_server();

#endif