#ifndef HEADER_FILE_NAME_H
#define HEADER_FILE_NAME_H

#define MAX_LEN 50

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_ops.h"
#include "diff.h"
#include <pthread.h>
#include "Server.h"
#include "glthread.h"
#include <poll.h>


GLTHREAD_TO_STRUCT(thread_TLV, TLV, glthread);

extern glthread_t file_list;
extern struct pollfd poll_fds[MAX_POLL_EVENTS];

// Definition of t_TLV
typedef struct t_TLV {
    int tlv;                   // The TLV number (0-255)
    char data[MAX_LEN];        // Data for the TLV
} t_TLV;

// Definition of TLVArray
typedef struct TLVArray {
    t_TLV* array;              // Pointer to a dynamic array of t_TLV elements
    int size;                  // Current number of elements in the array
    pthread_rwlock_t small_lock;   // Reader-writer lock for this TLVArray
} TLVArray;

// Define the SharedArray structure
#define TOTAL_TLVs 256
typedef struct SharedArray {
    TLVArray shared_arr[TOTAL_TLVs];  // Array of TLVArray
    pthread_rwlock_t big_lock;          // Reader-writer lock for the entire shared array
} SharedArray;


extern SharedArray shared_arr;

// Function to initialize the shared array
void init_SharedArray(SharedArray* shared_array);

// Function to free the shared array
void free_shared_array(SharedArray* shared_array);

// Function to write deserialized data into the shared array
void write_shared_array(SharedArray* shared_array, DiffBuffer* deserialized_diff);

// Function to print the shared array contents
void print_shared_arr(SharedArray* shared_array);

void* process_shared_arr(void* arg);

void free_TLVArray(TLVArray* tlv_array);

#endif