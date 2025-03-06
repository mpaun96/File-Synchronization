#ifndef DIFF_H
#define DIFF_H

#include "glthread.h"
#define MAX_LEN 50
#define MAX_DIF_NO 255

// Define the tlv structure here
typedef struct TLV {
    int tlv;
    char data[MAX_LEN];
    glthread_t glthread;
} TLV;

typedef struct DiffBuffer {
    TLV *buffer;
    int size;
} DiffBuffer;


// Declare the functions that are defined in diff.c
void display_buffer(DiffBuffer* buff);
DiffBuffer get_glthread_diff(glthread_t *list1, glthread_t *list2, int offset);
const char* serialize_diffbuffer(const DiffBuffer* diff, int* message_size);
DiffBuffer* deserialize_diffbuffer(const char *message, int message_size);

#endif // DIFF_H
