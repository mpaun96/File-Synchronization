#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <sys/time.h>  // Include this header for struct timeval
#include <errno.h>
#include "glthread.h"
#include "diff.h"
#include "client.h"


//Monitorting file stuff
int keycmp(void* T1, void* T2){
    TLV* t1 = (TLV*)T1;
    TLV* t2 = (TLV*)T2;
    if(t1->tlv == t2->tlv) return 0;
    if(t1->tlv < t2->tlv) return -1;
    return 1;
}

void write_glthread_to_file(const char *filename, glthread_t *glthread_head) {
    // Open the file in "w" mode to clear any existing content
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Could not open file %s for writing\n", filename);
        return;
    }

    // Iterate through the glthread list and write each entry to the file
    glthread_t *curr = NULL;
    ITERATE_GLTHREAD_BEGIN(glthread_head, curr) {
        TLV *p = thread_TLV(curr);
        fprintf(file, "TLV-%d: %s\n", p->tlv, p->data);
    } ITERATE_GLTHREAD_END(glthread_head, curr);

    printf("File %s has been updated with glthread data.\n", filename);
    // Close the file
    fclose(file);
}

char* serialize_glthread(glthread_t *glthread_head, int *message_size) {
    if (glthread_head == NULL || message_size == NULL) {
        return NULL; // Error handling
    }

    // Step 1: Manually count the nodes in the glthread list
    int node_count = 0;
    glthread_t *curr = NULL;
    ITERATE_GLTHREAD_BEGIN(glthread_head, curr) {
        node_count++;
    } ITERATE_GLTHREAD_END(glthread_head, curr);

    // Step 2: Calculate the total serialized size (based only on TLVs)
    *message_size = node_count * sizeof(TLV);
    char *serialized = (char *)malloc(*message_size);
    if (!serialized) {
        perror("malloc");
        exit(EXIT_FAILURE); // Handle memory allocation failure
    }

    // Step 3: Serialize each TLV node
    char *ptr = serialized; // Start at the beginning of the buffer
    TLV *tlv = NULL;

    ITERATE_GLTHREAD_BEGIN(glthread_head, curr) {
        tlv = thread_TLV(curr);
        memcpy(ptr, tlv, sizeof(TLV)); // Copy the entire TLV structure
        ptr += sizeof(TLV);
    } ITERATE_GLTHREAD_END(glthread_head, curr);

    return serialized;  // Return the serialized buffer
}

void deserialize_glthread(const char *serialized_data, int message_size, glthread_t *glthread_head) {
    if (!serialized_data || message_size <= 0 || !glthread_head) {
        return; // Handle invalid input
    }

    // Step 1: Calculate node count based on message size
    int node_count = message_size / sizeof(TLV);

    const char *ptr = serialized_data; // Start at the beginning of the buffer

    // Step 2: Deserialize each TLV and add to glthread list
    for (int i = 0; i < node_count; i++) {
        TLV *new_node = (TLV *)calloc(1, sizeof(TLV));
        if (!new_node) {
            perror("calloc");
            exit(EXIT_FAILURE);  // Handle memory allocation failure
        }

        memcpy(new_node, ptr, sizeof(TLV)); // Deserialize TLV
        ptr += sizeof(TLV);

        // Insert the deserialized node into the glthread list
        glthread_priority_insert(glthread_head, &new_node->glthread, keycmp, offset(TLV, glthread));
    }
}

void parseFile(const char *filename, glthread_t *glthread_head) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Could not open file %s\n", filename);
        //exit(1);
        return;
    }

    char line[MAX];
    while (fgets(line, sizeof(line), file)) {
        int tlv;
        char data[MAX];

        // Parse the line to extract TLV and the associated data
        if (sscanf(line, "TLV-%d: %255[^\n]", &tlv, data) == 2) {
            TLV* node = (TLV*)calloc(1,sizeof(TLV));
            node->tlv = tlv;
            strcpy(node->data, data);
            glthread_priority_insert(glthread_head, &node->glthread, keycmp, offset(TLV, glthread));
        } else {
            printf("Failed to parse line: %s\n", line);
        }
    }

    fclose(file);
}

void monitor_file(const char* filename) {
    int fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    int wd = inotify_add_watch(fd, filename, IN_MODIFY);
    if (wd == -1) {
        printf("Could not watch %s\n", filename);
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }

    printf("*********************************Monitoring %s for changes*********************************\n", filename);

    char buffer[EVENT_BUF_LEN];
    while (1) {
        int length = read(fd, buffer, EVENT_BUF_LEN);
        if (length < 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (atomic_load(&is_listener_writing) == true) {
            // If flagged, ignore changes
            printf("Modifications ignored.\n");
            continue;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            if (event->mask & IN_MODIFY) {
                printf("File %s was modified.\n", event->name);
            }
            i += EVENT_SIZE + event->len;
        }
        break;
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}

void apply_diff_and_modify_file(DiffBuffer *deserialized_diff, const char *filename) {

    // Open the file and replace its content
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Write each TLV entry from deserialized_diff->buffer into the file
    for (int i = 0; i < deserialized_diff->size; i++) {
        TLV *entry = &deserialized_diff->buffer[i];
        fprintf(file, "TLV-%d: %s\n", entry->tlv, entry->data);
    }

    fclose(file);
    printf("File modification complete.\n");

}


void* tcp_client_thread(void *args) {
    thread_args_t* params = (thread_args_t *)args;

    //Create a socket per thread
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return NULL;
    }
    struct sockaddr_in server_addr = params->server_addr;
 
    // Attempt to connect to the server with a timeout
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed");
        close(sockfd);
        return NULL;
    }
    printf("Connected to the server %s:%d\n", inet_ntoa(server_addr.sin_addr),  ntohs(server_addr.sin_port));
    
    // Prepare the message to send, serialize the message
    printf("Content of the buffer before being serialized and sent\n");
    display_buffer(&params->diff);
    printf("Number of changes: %d\n", params->diff.size);
    int message_size = 0;
    char* message = serialize_diffbuffer(&(params->diff), &message_size);

    //Append the message type
    append_type(&message, MESSAGE_TYPE_DATA, &message_size);

    // Send data until acknowledgment is received
    unsigned char type;
    char* response_buffer = (char*)malloc(BUFFER_SIZE);
    int bytes_received;

    do {
        // Send the message to the client
        ssize_t bytes_sent = send(sockfd, message, message_size, 0);
        if (bytes_sent < 0) {
            perror("Send failed");
            break;
        }
        printf("Sent %zd bytes to server\n", bytes_sent);

        // Wait for acknowledgment from the client
        bytes_received = recv(sockfd, response_buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("Receive failed");
            break;
        }

        // Strip the type from the response
        strip_type(&response_buffer, &type, &bytes_received);

    } while (type != MESSAGE_TYPE_ACK);

    printf("Got MESSAGE_TYPE_ACK from server.\n");

    // Free the test deserialization and message buffer and close the sockets
    close(sockfd);
    free(response_buffer);
    free(message);
    pthread_exit(NULL);
}


void error_exit(const char *msg) {
    perror(msg);
    pthread_exit(NULL);
}

void* sync_thread(void *args) {
    thread_args_t* params = (thread_args_t *)args;

    // Create a socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Sync thread: Socket creation error");
        return NULL;
    }
    
    struct sockaddr_in server_addr = params->server_addr;

    // Attempt to connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Sync thread: Connection Failed");
        close(sockfd);
        return NULL;
    }

    printf("Sync thread: Connected to the server %s:%d\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    // Prepare the sockaddr_in message to send
    struct sockaddr_in message_addr;
    memset(&message_addr, 0, sizeof(message_addr)); // Zero out the structure
    message_addr.sin_family = AF_INET; // Address family
    message_addr.sin_port = htons(LISTEN_PORT); // Port number (convert to network byte order)
    message_addr.sin_addr.s_addr = inet_addr(MY_LISTEN_IP); // IP address (convert to network byte order)
    
    // Serialize the message
    int message_size = 0;
    char *message = serialize_sockaddr_in(&message_addr, &message_size);
    if (message == NULL) {
        close(sockfd);
        return NULL; 
    }

    // Append the message type for synchronization
    append_type(&message, MESSAGE_TYPE_SYNC, &message_size);

    // Send the message continuously until an ACK is received
    while (1) {
        int bytes_sent = send(sockfd, message, message_size, 0);
        if (bytes_sent < 0) {
            perror("Send failed");
            break;
        }
        printf("Sent %d bytes to the server\n", bytes_sent);

        // Receive response from the server
        char* response_buffer = (char*)malloc(BUFFER_SIZE);
        if (response_buffer == NULL) {
            perror("Failed to allocate response buffer");
            break;
        }
        int bytes_received = recv(sockfd, response_buffer, sizeof(response_buffer), 0);
        if (bytes_received < 0) {
            perror("Receive failed");
            break;
        } else if (bytes_received == 0) {
            printf("Server closed connection\n");
            break; 
        }

        // Strip the type from the received response
        unsigned char response_type;
        strip_type(&response_buffer, &response_type, &bytes_received);


        // Check for acknowledgment
        if (response_type == MESSAGE_TYPE_ACK) {
            printf("Received ACK from server\n");

            //Signal the main thread
            pthread_mutex_lock(&sync_mutex);
            sync_complete = true;  // Set the flag indicating sync is complete
            pthread_cond_signal(&sync_cond);
            pthread_mutex_unlock(&sync_mutex);

            struct sockaddr_in* listen_socket = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
            memcpy(listen_socket, &message_addr, sizeof(struct sockaddr_in));

            pthread_t server_thread;
            // Create and start the server thread
            if (pthread_create(&server_thread, NULL, tcp_server_thread, (void*)listen_socket) != 0) {
                perror("Thread creation failed");
                free(listen_socket);
                free(message);
                pthread_exit(NULL);
            }
            
            // Detach the thread so that its resources are freed upon completion
            if (pthread_detach(server_thread) != 0) {
                perror("Thread creation failed");
                free(listen_socket);
                free(message);
                pthread_exit(NULL);
            }

            break;
        }
        free(response_buffer);
    }

    // Clean up
    close(sockfd);
    pthread_exit(NULL);
}

// Thread function to run the TCP server
void* tcp_server_thread(void* args) {
    struct sockaddr_in* server_addr = (struct sockaddr_in*)args;
    int server_socket = 0, client_socket = 0;
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);

    // Create a socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation failed");
        pthread_exit(NULL);
    }

    // Set socket options (like SO_REUSEADDR) for better behavior
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Failed to set socket options");
        close(server_socket);
        pthread_exit(NULL);
    }

    // Bind the socket to the provided IP and port
    if (bind(server_socket, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        printf("Bind failed");
        close(server_socket);
        pthread_exit(NULL);
    }

    // Start listening for connections
    if (listen(server_socket, BACKLOG) < 0) {
        printf("Listen failed");
        close(server_socket);
        pthread_exit(NULL);
    }

    printf("Listening on %s:%d for updates from server\n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port));

    // Accept connections in a loop
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        if (client_socket < 0) {
            perror("Accept failed");
            printf("Error code: %d\n", errno);
            continue;
        }

        printf("Accepted connection from server: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        atomic_store(&is_listener_writing, true);
        sleep(1);

        // Allocate buffer for receiving data
        char *serialized_buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
        if (!serialized_buffer) {
            perror("malloc");
        }
        memset(serialized_buffer, 0, BUFFER_SIZE);
        

        int bytes_received = recv(client_socket, serialized_buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("Receive failed");
            free(serialized_buffer);
            close(client_socket);
            continue;
        }
        
        int buffer_size = bytes_received;
        //Strip the type
        unsigned char type;
        strip_type(&serialized_buffer, &type, &buffer_size);
        
        if(type == MESSAGE_TYPE_DATA) {
            
            // Write the content to file
            glthread_t filelist1;
            init_glthread(&filelist1);
            deserialize_glthread(serialized_buffer, buffer_size, &filelist1);
            
            printf("Updating the file with data from server:\n");
            glthread_t *curr = NULL;
            ITERATE_GLTHREAD_BEGIN(&filelist1, curr) {
                TLV *p = thread_TLV(curr);
                printf("TLV = %d; data = %s\n", p->tlv, p->data);
            } ITERATE_GLTHREAD_END(&filelist1, curr);
             
             write_glthread_to_file("test.txt", &filelist1);
             
             // Create a thread to send the ACK
             int *socket_ptr = malloc(sizeof(int));
             if (socket_ptr == NULL) {
                perror("malloc");
                close(client_socket);
                continue;
                }
            *socket_ptr = client_socket;
            pthread_t ack_thread;
            if (pthread_create(&ack_thread, NULL, send_ack, (void*)socket_ptr) != 0) {
                perror("Failed to create ACK thread");
                free(socket_ptr);
                close(client_socket);
                continue;
                }
            
            // Detach
            pthread_detach(ack_thread);

            free(serialized_buffer);
        }

    }

    // Close the server socket when the server shuts down
    close(client_socket);
    close(server_socket);
    pthread_exit(NULL);
}