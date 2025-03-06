#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_POLL_EVENTS 2
#define MAX_CLIENTS 10
#define SERVER_PORT 8080

pthread_mutex_t array_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;

int server_socket, client_socket;
socklen_t addr_len = sizeof(struct sockaddr_in);

// Shared resource
void* shared_array = NULL;

// Handle client connections (stub)
void* handle_client(void* arg) {
    // Client-specific logic here
    return NULL;
}

// Process the 'start' command
void* process_start(void* arg) {
    pthread_mutex_lock(&array_mutex);

    // Wait for shared_array to be initialized before running
    if (shared_array == NULL) {
        printf("Cannot start: shared_array is NULL\n");
        pthread_mutex_unlock(&array_mutex);
        return NULL;
    }

    // Perform the 'start' operation
    printf("Processing 'start' command...\n");
    // Simulate some work
    sleep(2);  // Simulated work

    printf("'start' completed\n");

    pthread_mutex_unlock(&array_mutex);

    // Signal the barrier, allowing the 'sync' thread to proceed
    pthread_barrier_wait(&barrier);
    return NULL;
}

// Process the 'sync' command
void* process_sync(void* arg) {
    printf("Waiting for 'start' to complete...\n");

    // Wait until 'start' thread reaches the barrier
    pthread_barrier_wait(&barrier);

    // Perform the 'sync' operation
    printf("Processing 'sync' command...\n");
    // Simulate some work
    sleep(2);  // Simulated work

    printf("'sync' completed\n");
    return NULL;
}

int main() {
    struct pollfd fds[MAX_POLL_EVENTS];
    struct sockaddr_in server_addr, client_addr;
    pthread_t client_threads[MAX_CLIENTS];
    pthread_t start_thread, sync_thread;
    int client_count = 0;

    // Initialize the barrier for 2 threads: 'start' and 'sync'
    pthread_barrier_init(&barrier, NULL, 2);

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    fds[0].fd = server_socket;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    while (1) {
        int poll_count = poll(fds, MAX_POLL_EVENTS, -1);
        if (poll_count < 0) {
            perror("poll");
            break;
        }

        // Handle server socket (incoming client connections)
        if (fds[0].revents & POLLIN) {
            client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
            if (client_socket < 0) {
                perror("accept");
                continue;
            }

            // Create thread to handle client
            if (pthread_create(&client_threads[client_count++], NULL, handle_client, NULL) != 0) {
                perror("pthread_create");
                close(client_socket);
                continue;
            }

            if (client_count >= MAX_CLIENTS) {
                printf("Max clients reached\n");
                break;
            }
        }

        // Handle stdin input
        if (fds[1].revents & POLLIN) {
            char command[10];
            fgets(command, sizeof(command), stdin);

            // 'start' command logic
            if (strncmp(command, "start", 5) == 0) {
                pthread_mutex_lock(&array_mutex);

                // Disable stdin polling temporarily while 'start' runs
                if (shared_array == NULL) {
                    printf("'start' command ignored: shared_array is NULL\n");
                } else {
                    fds[1].events &= ~POLLIN;
                    pthread_create(&start_thread, NULL, process_start, NULL);
                    pthread_detach(start_thread);
                }

                pthread_mutex_unlock(&array_mutex);
            }
            // 'sync' command logic
            else if (strncmp(command, "sync", 4) == 0) {
                // Sync thread will wait for start to complete via barrier
                pthread_create(&sync_thread, NULL, process_sync, NULL);
                pthread_detach(sync_thread);
            } else {
                // Invalid input handling
                printf("Invalid input: %s", command);
            }
        }
    }

    // Clean up server socket
    close(server_socket);

    // Destroy the barrier
    pthread_barrier_destroy(&barrier);

    return 0;
}
