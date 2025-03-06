#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <stdatomic.h>  // Include the atomic operations header
#include <stdbool.h>     // For using bool type in C
#include <pthread.h>
#include <sys/time.h>  // Include this header for struct timeval
#include <errno.h>
#include "glthread.h"
#include "diff.h"
#include "client.h"
#include "types.h"

atomic_bool is_listener_writing = false;

// Global synchronization variables
pthread_mutex_t sync_mutex;       // Mutex for sync control
pthread_cond_t sync_cond;         // Condition variable for sync completion
bool sync_complete = false;       // Flag indicating if sync is complete

thread_args_t* setup_server_info() {
    thread_args_t* args = (thread_args_t*)malloc(sizeof(thread_args_t));
    if (!args) {
        perror("Failed to allocate memory for thread_args_t");
        exit(EXIT_FAILURE);
    }
    memset(args, 0, sizeof(thread_args_t));
    args->server_addr.sin_family = AF_INET;
    args->server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &args->server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        free(args); // Free allocated memory before exit
        exit(EXIT_FAILURE);
    }
    return args;
}

void monitor_loop(thread_args_t* args) {
    glthread_t dlist1, dlist2;
    pthread_t client_thread;

    while (1) {
        init_glthread(&dlist1);
        init_glthread(&dlist2);
        
        parseFile("test.txt", &dlist1);
        monitor_file("test.txt");
        parseFile("test.txt", &dlist2);
        printf("File was modified!\n");

        args->diff.size = 0;
        args->diff.buffer = NULL;
        args->diff = get_glthread_diff(&dlist1, &dlist2, offset(TLV, glthread));

        if (args->diff.size != 0 && args->diff.buffer != NULL) {

            // Create the thread
            if (pthread_create(&client_thread, NULL, tcp_client_thread, (void*)args) != 0) {
                perror("Failed to create thread");
                exit(EXIT_FAILURE);
            }

            if (pthread_join(client_thread, NULL) != 0) {
                perror("Failed to join the thread");
            }
        }

        args->diff.size = 0;
        free(args->diff.buffer);
        args->diff.buffer = NULL;
        delete_glthread_list(&dlist1);
        delete_glthread_list(&dlist2);
    }
}



int main(){

    // Initialize mutex and condition variable
    if (pthread_mutex_init(&sync_mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&sync_cond, NULL) != 0) {
        perror("Failed to initialize condition variable");
        pthread_mutex_destroy(&sync_mutex); // Clean up before exiting
        exit(EXIT_FAILURE);
    }

    // Create and initialize thread arguments
    thread_args_t* args = setup_server_info();

    //Create the sync thread(that launches the listener thread)
    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, sync_thread, (void*)args) != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    // Main thread waits for the sync thread to complete
    printf("Waiting for the sync to finish...\n");
    pthread_mutex_lock(&sync_mutex);
    while (!sync_complete) {
        pthread_cond_wait(&sync_cond, &sync_mutex);  // Wait for sync to complete
    }
    pthread_mutex_unlock(&sync_mutex);
   
    // Call the monitor loop
    monitor_loop(args);

    // Cleanup resources
    if (pthread_join(server_thread, NULL) != 0) {
        perror("Failed to join server thread");
    }
    // Free the allocated arguments
    free(args);
    // Destroy the condition variable and mutex with error checking
    if (pthread_cond_destroy(&sync_cond) != 0) {
        perror("Failed to destroy condition variable");
    }
    if (pthread_mutex_destroy(&sync_mutex) != 0) {
        perror("Failed to destroy mutex");
    }
    return 0;
}