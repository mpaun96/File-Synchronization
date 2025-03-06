void* tcp_client_thread(void *args) {
    printf("THEAD IS CREATED!!!!!");
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

    int message_size = 0;
    char *message = serialize_diffbuffer(&(params->diff), &message_size);
    printf("####DEBUG TYPES!! size after serialize %d\n", message_size);

    //Append the message type
    append_type(message, MESSAGE_TYPE_DATA, &message_size);
    printf("####DEBUG TYPES!! size after append to serialized %d\n", message_size);

    
    //send the message
    int bytes_sent = send(sockfd, message, message_size, 0);
    if (bytes_sent < 0) {
        error_exit("Send failed");
    }

    printf("Sent %zd bytes to the server\n", bytes_sent);

    // Free the test deserialization and message buffer and close the sockets
    close(sockfd);

    pthread_exit(NULL);
}