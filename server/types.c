#include "types.h"

/*
void append_type(char *serialized_data, unsigned char type, int *size) {
    if (serialized_data == NULL || size == NULL) {
        return;
    }

    memmove(serialized_data + 1, serialized_data, *size);
    serialized_data[0] = type;
    (*size)++;
}

void strip_type(char **serialized_data, unsigned char *type, int *remaining_size) {
    if (serialized_data == NULL || *serialized_data == NULL || type == NULL || remaining_size == NULL) {
        return;
    }

    *type = (*serialized_data)[0];
    (*remaining_size)--;
    *serialized_data += 1;
}
*/

void append_type(char **serialized_data, unsigned char type, int *size) {
    if (serialized_data == NULL || *serialized_data == NULL || size == NULL) {
        return;
    }

    // Create a new buffer for the serialized data with an additional byte for the type
    char *new_buffer = (char *)malloc((*size + 1) * sizeof(char));
    if (new_buffer == NULL) {
        perror("Failed to allocate memory for new buffer");
        return;
    }

    // Set the first byte to type
    new_buffer[0] = type;
    // Copy the existing data to the new buffer
    memcpy(new_buffer + 1, *serialized_data, *size);

    // Free the old buffer
    free(*serialized_data);
    *serialized_data = new_buffer;
    (*size)++; // Increment the size
}

void strip_type(char **serialized_data, unsigned char *type, int *remaining_size) {
    if (serialized_data == NULL || *serialized_data == NULL || type == NULL || remaining_size == NULL) {
        return;
    }

    *type = (*serialized_data)[0]; // Get the type byte
    (*remaining_size)--; // Decrease the size
    char *new_buffer = (char *)malloc((*remaining_size) * sizeof(char));
    if (new_buffer == NULL) {
        perror("Failed to allocate memory for new buffer after stripping type");
        return;
    }

    // Copy the remaining data (excluding the type byte)
    memcpy(new_buffer, *serialized_data + 1, *remaining_size);

    // Free the old buffer
    free(*serialized_data);
    *serialized_data = new_buffer; // Update the pointer to the new buffer
}


char* serialize_sockaddr_in(struct sockaddr_in *input, int *size) {
    if (input == NULL || size == NULL) {
        return NULL;  // Error handling
    }

    // Allocate memory for serialized data
    char* serialized_data = malloc(sizeof(struct sockaddr_in));
    if (serialized_data == NULL) {
        perror("Memory allocation failed");
        return NULL;  // Error handling
    }

    // Copy the data to the serialized format
    memcpy(serialized_data, input, sizeof(struct sockaddr_in));
    *size = sizeof(struct sockaddr_in);  // Set the size

    return serialized_data;  // Return the serialized data
}


struct sockaddr_in deserialize_sockaddr_in(char *serialized_data) {
    struct sockaddr_in output;

    if (serialized_data == NULL) {
        memset(&output, 0, sizeof(output));  // Handle error by returning zeroed struct
        return output;
    }

    // Copy the data back to the sockaddr_in structure
    memcpy(&output, serialized_data, sizeof(struct sockaddr_in));
    return output;  // Return the deserialized sockaddr_in
}


// Function to send an acknowledgment back to the client
void* send_ack(void* arg) {
    int client_socket = *(int*)arg; // Cast the argument back to int
    free(arg); // Free the allocated memory for the socket

    // Prepare the acknowledgment message
    char* ack_message = (char*)malloc(sizeof(char) * 1); // Initialize the ACK message
    ack_message[0] = '0';
    int message_size = 0; // Size for the append function

    // Append the MESSAGE_TYPE_ACK to the message
    append_type(&ack_message, MESSAGE_TYPE_ACK, &message_size);

    // Send the acknowledgment back to the client
    ssize_t bytes_sent = send(client_socket, ack_message, message_size, 0);
    if (bytes_sent < 0) {
        perror("send");
        pthread_exit(NULL); // Exit the thread if sending fails
    } else {
        printf("Sent MESSAGE_TYPE_ACK to the client.\n");
    }
    //atomic_store(&is_listener_writing, false);
    pthread_exit(NULL);; // Exit the thread successfully
}
