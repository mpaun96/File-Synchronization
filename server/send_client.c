#include "send_client.h"
#include <stdio.h>  // Includes NULL
#include <stdlib.h> // Includes NULL

int ACK_COUNT = 0;

// Compare function to check if two sockaddr_in structs are equal
int compare_sockaddr(const struct sockaddr_in *addr1, const struct sockaddr_in *addr2) {
    return (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr) && 
           (addr1->sin_port == addr2->sin_port);
}

int get_client_count(client_list *cl_list) {
    pthread_mutex_lock(&cl_list->mutex);  // Lock the mutex for thread-safe operations

    glthread_t *curr_node = NULL;
    int count = 0;

    // Iterate through the linked list to count the number of clients
    ITERATE_GLTHREAD_BEGIN(&cl_list->head, curr_node) {
        count++;
    } ITERATE_GLTHREAD_END(&cl_list->head, curr_node);

    pthread_mutex_unlock(&cl_list->mutex);  // Unlock the mutex after operation
    return count;  // Return the count of clients
}

void print_clients(client_list *cl_list) {
    pthread_mutex_lock(&cl_list->mutex);  // Lock the mutex for thread-safe operations

    glthread_t *curr_node = NULL;
    int count = 0;

    ITERATE_GLTHREAD_BEGIN(&cl_list->head, curr_node) {
        client_node *client = thread_cl_node(curr_node);  // Get the client_node from the glthread node
        struct sockaddr_in client_addr = client->client_addr;

        // Convert IP address and port to readable format
        char *client_ip = inet_ntoa(client_addr.sin_addr);  // Convert IP to string
        int client_port = ntohs(client_addr.sin_port);      // Convert port to host byte order

        printf("Client %d, listening on IP: %s, Port: %d\n", ++count, client_ip, client_port);
    } ITERATE_GLTHREAD_END(&cl_list->head, curr_node);

    pthread_mutex_unlock(&cl_list->mutex);  // Unlock the mutex after operation
}


// Add a client to the client list
void add_client(client_list *cl_list, const struct sockaddr_in *client_addr) {
    pthread_mutex_lock(&cl_list->mutex);  // Lock the mutex for thread-safe operations

    // Check if the client already exists (by IP and port)
    glthread_t *curr_node = NULL;
    int found = 0;

    ITERATE_GLTHREAD_BEGIN(&cl_list->head, curr_node) {
        client_node *client = thread_cl_node(curr_node);
        if (compare_sockaddr(&client->client_addr, client_addr)) {
            // Client already exists, replace content if needed
            client->client_addr = *client_addr;
            found = 1;
            break;
        }
    } ITERATE_GLTHREAD_END(&cl_list->head, curr_node);

    // If the client was not found, add it to the list
    if (!found) {
        client_node *new_node = (client_node *)calloc(1, sizeof(client_node));
        if (!new_node) {
            perror("Memory allocation failed");
            pthread_mutex_unlock(&cl_list->mutex);  // Unlock before returning
            return;
        }
        // Initialize the new node
        new_node->client_addr = *client_addr;
        init_glthread(&new_node->glthread);

        // Add the new node right after the last node checked
        if (curr_node == NULL) {
            // List was empty, so add as the first element
            glthread_add_next(&cl_list->head, &new_node->glthread);
        } else {
            // Add after the last node
            glthread_add_next(curr_node, &new_node->glthread);
        }

        // Print client IP and port for debugging
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr->sin_addr, ip_str, sizeof(ip_str));
        printf("Added client: IP = %s, Port = %d\n", 
               ip_str, ntohs(client_addr->sin_port));
    }

    pthread_mutex_unlock(&cl_list->mutex);  // Unlock the mutex after operation
}

int keycmp(void* T1, void* T2){
    TLV* t1 = (TLV*)T1;
    TLV* t2 = (TLV*)T2;
    if(t1->tlv == t2->tlv) return 0;
    if(t1->tlv < t2->tlv) return -1;
    return 1;
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

    //printf("Deserializing node count (calculated): %d\n", node_count);
    //printf("deserialize_glthread: message size = %d\n", message_size);

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

// Sender thread function
void* sender_thread(void *args) {
    sender_thread_args_t *thread_args = (sender_thread_args_t *)args;

    // Create a new socket for the client
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        free(thread_args);
        pthread_exit(NULL);
    }

    // Attempt to connect to the client
    if (connect(sockfd, (struct sockaddr *)&thread_args->client_addr, sizeof(thread_args->client_addr)) < 0) {
        perror("Connection to client failed");
        close(sockfd);
        free(thread_args);
        pthread_exit(NULL);
    }

    printf("Connected to client: %s:%d\n", inet_ntoa(thread_args->client_addr.sin_addr), ntohs(thread_args->client_addr.sin_port));
/*
    // Make a copy of the serialized data to avoid modifying the original pointer
    int data_size = thread_args->data_size;
    char* data_copy = (char*)malloc(data_size * sizeof(char)); // Copy the data so it doesn't get corrupted
    if (data_copy == NULL) {
        perror("Memory allocation failed");
        close(sockfd);
        free(thread_args);
        pthread_exit(NULL);
    }
    memcpy(data_copy, thread_args->serialized_data, data_size);  // Copy the data

    // Now strip the type from the copied data, not the original
    unsigned char message_type;
    strip_type(&data_copy, &message_type, &data_size);

    if (message_type == MESSAGE_TYPE_DATA) {
        printf("Got type MESSAGE_TYPE_DATA BEFORE SEND!!!!\n");
    }


    // Deserialize the copied data for testing purposes
    glthread_t file_list_test;
    glthread_t* curr;
    init_glthread(&file_list_test);
    deserialize_glthread(data_copy, data_size, &file_list_test);

    printf("CONTENT OF THE MESSAGE BEFORE BEING SENT:\n");
    ITERATE_GLTHREAD_BEGIN(&file_list_test, curr) {
        TLV *p = thread_TLV(curr);
        printf("TLV = %d; data = %s\n", p->tlv, p->data);
    } ITERATE_GLTHREAD_END(&file_list_test, curr);
*/
    // Send data until acknowledgment is received
    unsigned char ack_type;
    do {
        char* response_buffer = (char*)malloc(BUFFER_SIZE);
        int bytes_received;

        // Send the copied message to the client
        ssize_t bytes_sent = send(sockfd, thread_args->serialized_data, thread_args->data_size, 0);
        if (bytes_sent < 0) {
            perror("Send failed");
            break;
        }
        printf("Sent %zd bytes to client\n", bytes_sent);

        // Wait for acknowledgment from the client
        bytes_received = recv(sockfd, response_buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("Receive failed");
            break;
        }

        // Strip the type from the response
        strip_type(&response_buffer, &ack_type, &bytes_received);

    } while (ack_type != MESSAGE_TYPE_ACK);

    ACK_COUNT++;
    printf("Acknowledgment number %d received from client!\n",ACK_COUNT);

    // Clean up
    close(sockfd);
    //free(data_copy);  // Free the copied data
    free(thread_args);

    pthread_exit(NULL);
}


// Function to launch sender threads for all clients
void broadcast_to_clients(client_list *cl_list, char *serialized_data, int data_size) {
    glthread_t *curr_node = NULL;
    ITERATE_GLTHREAD_BEGIN(&cl_list->head, curr_node) {
        client_node *client = thread_cl_node(curr_node);

        // Prepare thread arguments
        sender_thread_args_t *args = (sender_thread_args_t *)malloc(sizeof(sender_thread_args_t));
        if (!args) {
            perror("Failed to allocate memory for thread args");
            continue;  // Move to the next client if memory allocation fails
        }

        args->client_addr = client->client_addr;
        args->serialized_data = (char*)malloc(sizeof(char) * data_size);  // Assuming this data is safe to reuse across threads
        memcpy(args->serialized_data, serialized_data, data_size);
        args->data_size = data_size;
/*
        //test append strip.
        int test_size = args->data_size;
        char* test_string = (char*)malloc(test_size * sizeof(char));
        memcpy(test_string, args->serialized_data, test_size);
        
        unsigned char test_type;
        strip_type(&test_string, &test_type, &test_size);
        
        if(test_type == MESSAGE_TYPE_DATA){
            printf("!!!!!!Got type MESSAGE_TYPE_DATA in broadcast_function!!!!\n");
        }
        
        glthread_t file_list_test;
        glthread_t* curr;
        init_glthread(&file_list_test);
        deserialize_glthread(test_string, test_size, &file_list_test);
        
        printf("-----------> Content of the meesage before being in broadcasted to all clients:\n");
        ITERATE_GLTHREAD_BEGIN(&file_list_test, curr){
            TLV *p = thread_TLV(curr);
            printf("TLV = %d; data = %s\n", p->tlv, p->data);
        } ITERATE_GLTHREAD_END(&file_list_test, curr);
*/
        // Create and detach the sender thread for each client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, sender_thread, (void*)args) != 0) {
            perror("Failed to create sender thread");
            free(args);
        } else {
            pthread_detach(thread_id);  // Detach the thread to run independently
        }

    } ITERATE_GLTHREAD_END(&cl_list->head, curr_node);
}

void sync_function() {
    // Assuming file_list is global, we don't pass it as an argument.
    // Also, assuming cl_list is global.

    // Step 1: Serialize the file list.

    printf("-----------> Content of the message before serializing and broadcasting to all clients:\n");
    glthread_t *curr = NULL;
        ITERATE_GLTHREAD_BEGIN(&file_list, curr){
            TLV *p = thread_TLV(curr);
            printf("TLV = %d; data = %s\n", p->tlv, p->data);
    } ITERATE_GLTHREAD_END(&file_list, curr);

    int serialized_size = 0;
    char *serialized_data = serialize_glthread(&file_list, &serialized_size);
    if (!serialized_data) {
        fprintf(stderr, "Failed to serialize the file list.\n");
        pthread_exit(NULL);
    }
/*
    //test append strip.
    int test_size = serialized_size;
    char* test_string = (char*)malloc(test_size * sizeof(char));
    memcpy(test_string, serialized_data, test_size);

    append_type(&test_string, MESSAGE_TYPE_DATA, &test_size);

    unsigned char type;
    strip_type(&test_string, &type, &test_size);

    if(type == MESSAGE_TYPE_DATA){
        printf("Got type MESSAGE_TYPE_DATA\n");
    }

    glthread_t file_list_test;
    init_glthread(&file_list_test);
    deserialize_glthread(test_string, test_size, &file_list_test);

    printf("after glthread_deserialize test:\n");
        ITERATE_GLTHREAD_BEGIN(&file_list_test, curr){
            TLV *p = thread_TLV(curr);
            printf("TLV = %d; data = %s\n", p->tlv, p->data);
    } ITERATE_GLTHREAD_END(&file_list_test, curr);
*/
    //append the data type
    append_type(&serialized_data, MESSAGE_TYPE_DATA, &serialized_size);

    // Step 2: Lock the mutex before accessing the client list.
    pthread_mutex_lock(&cl_list.mutex);

    // Step 3: Call the broadcast function to send serialized data to all clients.
    broadcast_to_clients(&cl_list, serialized_data, serialized_size);

    // Step 4: Unlock the mutex after the broadcast is complete.
    pthread_mutex_unlock(&cl_list.mutex);

    // Step 5: Free the serialized data.
    free(serialized_data);

    printf("-----------> All clients have been synchronized with the current version of the TLV FILE!\n");
    //pthread_exit(NULL);
}