#include "Server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include "diff.h"  // Replace with your actual header file containing DiffBuffer, serialize, deserialize, etc.
#include "CRUDshared_array.h"
#include <stdatomic.h>
#include "types.h"

extern SharedArray shared_arr;
//extern atomic_int thread_running;  // Atomic flag to trackl thread status
//extern atomic_int sync_ready;  // Atomic flag to track thread status
extern client_list cl_list;

void* handle_client(void* args) {
    //printf("-----------client thread has started-----------\n");
    client_thread_args_t* client_args = (client_thread_args_t*)args;
    int client_socket = client_args->client_socket;
    char* buffer = client_args->serialized_message;
    char ip_str[INET_ADDRSTRLEN];


    // Deserialize the received message
    int message_size = client_args->size_of_message;
    DiffBuffer* deserialized_diff = deserialize_diffbuffer(buffer, message_size);

    inet_ntop(AF_INET, &client_args->client_add.sin_addr, ip_str, sizeof(ip_str));
    printf("-----------> Client %s:%d trying to appent to sharred array <-----------\n",ip_str,ntohs(client_args->client_add.sin_port));
    // Critical Section Begin
    write_shared_array(&shared_arr, deserialized_diff);
    // Critical Section End

    // Create a thread to send the ACK
    int *socket_ptr = malloc(sizeof(int));
    if (socket_ptr == NULL) {
        perror("malloc");
        close(client_socket);
    }
    *socket_ptr = client_socket;
    pthread_t ack_thread;
    if (pthread_create(&ack_thread, NULL, send_ack, (void*)socket_ptr) != 0) {
        perror("Failed to create ACK thread");
        free(socket_ptr); // Free allocated memory on failure
        close(client_socket);
    }
    // Detach
    pthread_detach(ack_thread);

    printf("-----------> Display the buffer with updates from client:\n");
    display_buffer(deserialized_diff);
    printf("-----------> Display the shared array after client updates:\n");
    print_shared_arr(&shared_arr);


    // Clean up
    free(deserialized_diff->buffer);
    free(deserialized_diff);
    free(client_args);

    printf("-----------> Done with client %s:%d <-----------\n",ip_str,ntohs(client_args->client_add.sin_port));
    return NULL;
}

void start_server() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    pthread_t client_threads[MAX_CLIENTS];
    int client_count = 0;


    // Create server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Bind server socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);
    
    // Prepare for polling
    //struct pollfd poll_fds[MAX_POLL_EVENTS];
    poll_fds[0].fd = server_socket; // Monitor server socket
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = STDIN_FILENO; // Monitor stdin for commands
    poll_fds[1].events = POLLIN;

    while (1) {
        printf("Server waiting for events.....\n");
        int poll_count = poll(poll_fds, MAX_POLL_EVENTS, -1); // -1 for blocking, 0 for non-blocking
        if (poll_count < 0) {
            perror("poll");
            break;
        }


        if(poll_fds[0].revents & POLLIN){
            printf("########Got event on server socket!\n");
            // Accept incoming connection

            client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
            if (client_socket < 0) {
                perror("accept");
                continue;
            }

            // Allocate buffer for receiving data
            char *serialized_buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
            if (!serialized_buffer) {
                perror("malloc");
            }
            memset(serialized_buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(client_socket, serialized_buffer, BUFFER_SIZE, 0);
            if (bytes_received < 0) {
                perror("recv");
            }

            //Strip the type
            int buffer_size = bytes_received;
            unsigned char type;
            strip_type(&serialized_buffer, &type, &buffer_size);


            // Switch case based on message type
            switch (type) {
                case MESSAGE_TYPE_DATA:
                printf("########Got message type: MESSAGE_TYPE_DATA (1)\n");
                // Allocate memory for client thread arguments
                client_thread_args_t* client_args = (client_thread_args_t*)malloc(sizeof(client_thread_args_t));
                if (!client_args) {
                    perror("malloc");
                    continue;
                }
                client_args->client_socket= client_socket;
                client_args->client_add = client_addr;
                client_args->serialized_message = serialized_buffer;
                client_args->size_of_message = buffer_size;
                
                // Create a new thread for each client
                if((pthread_create(&client_threads[client_count++], NULL, handle_client, (void*)client_args) != 0)) {
                    perror("pthread_create");
                    close(client_socket);
                    free(client_args);
                    continue;
                }
                    // Detach the thread to allow it to clean up after itself
                pthread_detach(client_threads[client_count - 1]);
                //printf("thread detached\n");
                if (client_count >= MAX_CLIENTS) {
                    printf("Max clients reached. Waiting for threads to complete...\n");
                    break;
                }
                break;
                case MESSAGE_TYPE_SYNC:
                printf("########Got message type: MESSAGE_TYPE_SYNC (2)\n");
                struct sockaddr_in client =  deserialize_sockaddr_in(serialized_buffer);
                add_client(&cl_list, &client);
                int *socket_ptr = malloc(sizeof(int));
                if (socket_ptr == NULL) {
                    perror("malloc");
                    close(client_socket);
                    continue; // Handle allocation failure
                }
                *socket_ptr = client_socket;

                // Create a thread to send the ACK
                pthread_t ack_thread;
                if (pthread_create(&ack_thread, NULL, send_ack, (void*)socket_ptr) != 0) {
                    perror("Failed to create ACK thread");
                    free(socket_ptr); // Free allocated memory on failure
                    close(client_socket);
                    continue;  // Handle the error accordingly
                }
                // Detach
                pthread_detach(ack_thread);
                break;
                // Add other message types here as needed
                default:
                printf("########DEBUG TYPES!! got unknown type: %d\n", type);
                // Handle unknown/unsupported type
                break;
            }
        }

        if ((poll_fds[1].revents & POLLIN)) {
            // User input
            char command[10];
            if (fgets(command, sizeof(command), stdin) == NULL) {
                // Handle error: fgets failed or EOF was reached
                perror("fgets failed");
                break;
            }

            if (strncmp(command, "start", 5) == 0) {
                printf("GOT INPUT: %s\n", command);

                // Disable stdin polling temporarily
                poll_fds[1].events &= ~POLLIN;  // Turn off POLLIN for stdin

                // Create and start the reader thread
                pthread_t reader_tid;
                // Set thread_running to 1 atomically
                if (pthread_create(&reader_tid, NULL, process_shared_arr, (void*)&shared_arr) != 0) {
                    perror("pthread_create");
                    exit(EXIT_FAILURE);
                }
                 pthread_detach(reader_tid);
            } else if(strncmp(command, "print", 5) == 0){
                print_clients(&cl_list);
                printf("Got %d clients in total!\n", get_client_count(&cl_list));
            } else {
                // Invalid input, flush the stdin buffer
                memset(command, 0, sizeof(command));  // Set all elements to 0
                printf("Invalid input: %s", command);
                int ch;
                while ((ch = getchar()) != '\n' && ch != EOF); // Clear remaining characters
            }
        }
    }

    // Clean up and close the server socket
    close(server_socket);
}