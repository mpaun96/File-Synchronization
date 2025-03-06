#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "glthread.h"
#include "diff.h"

void display_buffer(DiffBuffer *diff_buffer) {
    for (int i = 0; i < diff_buffer->size; i++) {
        TLV *tlv = &diff_buffer->buffer[i];
        printf("TLV: %d, Data: %s\n", tlv->tlv, tlv->data);
    }
}


DiffBuffer get_glthread_diff(glthread_t *list1, glthread_t *list2, int offset) {
    DiffBuffer diff_buffer;
    diff_buffer.size = 0;
    diff_buffer.buffer = (TLV *)malloc(MAX_DIF_NO * sizeof(TLV)); // Allocate an initial buffer, adjust size as needed

    glthread_t *curr1 = list1->right;
    glthread_t *curr2 = list2->right;

    while (curr1 || curr2) {
        TLV *tlv1 = curr1 ? (TLV *)((char *)curr1 - offset) : NULL;
        TLV *tlv2 = curr2 ? (TLV *)((char *)curr2 - offset) : NULL;

        if (tlv1 && (!tlv2 || tlv1->tlv < tlv2->tlv)) {
            // Node exists in list1 but not in list2, record deletion
            TLV delete_tlv = *tlv1;
            strcpy(delete_tlv.data, "DELETE");
            diff_buffer.buffer[diff_buffer.size++] = delete_tlv;
            curr1 = curr1->right;
        } else if (tlv2 && (!tlv1 || tlv2->tlv < tlv1->tlv)) {
            // Node exists in list2 but not in list1, append it
            diff_buffer.buffer[diff_buffer.size++] = *tlv2;
            curr2 = curr2->right;
        } else if (tlv1 && tlv2 && tlv1->tlv == tlv2->tlv) {
            if (strcmp(tlv1->data, tlv2->data) != 0) {
                // Nodes are present in both lists with the same tlv but different data, append tlv2
                diff_buffer.buffer[diff_buffer.size++] = *tlv2;
            }
            curr1 = curr1->right;
            curr2 = curr2->right;
        }
    }

    return diff_buffer;
}

const char* serialize_diffbuffer(const DiffBuffer* diff, int* message_size) {
    // Calculate the total size needed for serialization
    *message_size = sizeof(int) + diff->size * sizeof(TLV);  // Fixed size for each TLV

    char* serialized = (char*)malloc(*message_size);
    if (serialized == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Copy the size of the TLV array
    memcpy(serialized, &diff->size, sizeof(int));

    //printf("INSIDE THE SERIALIZE FUNCTION:\n");

    // Copy TLV data
    char* ptr = serialized + sizeof(int);
    for (int i = 0; i < diff->size; ++i) {
        // Copy the entire TLV structure at once
        memcpy(ptr, &diff->buffer[i], sizeof(TLV));
        printf("TLV: %d, data: %s\n", diff->buffer[i].tlv, diff->buffer[i].data);
        ptr += sizeof(TLV);
    }
    //printf("SERIALIZE LOOP HAS FINISHED:\n");

    return serialized;
}


DiffBuffer* deserialize_diffbuffer(const char *message, int message_size) {

    // Step 1: Allocate memory for the DiffBuffer structure
    DiffBuffer *diff_buffer = (DiffBuffer *)malloc(sizeof(DiffBuffer));
    if (diff_buffer == NULL) {
        perror("Failed to allocate memory for DiffBuffer");
        return NULL;
    }

    // Step 2: Extract the number of TLVs (stored as an integer at the beginning)
    if (message_size < sizeof(int)) {
        fprintf(stderr, "Invalid message size: too small to contain size information.\n");
        free(diff_buffer);
        return NULL;
    }

    int num_tlvs;
    memcpy(&num_tlvs, message, sizeof(int));

    // Step 3: Allocate memory for the TLV array in DiffBuffer
    diff_buffer->buffer = (TLV *)malloc(num_tlvs * sizeof(TLV));
    if (diff_buffer->buffer == NULL) {
        perror("Failed to allocate memory for TLV array");
        free(diff_buffer);
        return NULL;
    }
    diff_buffer->size = num_tlvs;

    // Step 4: Iterate through the serialized data and fill the TLV array
    const char *current_position = message + sizeof(int);
    int expected_size = sizeof(int) + num_tlvs * sizeof(TLV);

    //printf("DEBUG!!!! message_size = %d &  expected_size = %d\n", message_size, expected_size);

    if (message_size < expected_size) {
        fprintf(stderr, "Invalid message size: does not match expected TLV array size.\n");
        free(diff_buffer->buffer);
        free(diff_buffer);
        return NULL;
    }

    //printf("INSIDE THE DE-SERIALIZE FUNCTION:\n");
    for (int i = 0; i < num_tlvs; i++) {
        memcpy(&diff_buffer->buffer[i], current_position, sizeof(TLV));
        //printf("getting TLV: %d, data: %s\n", diff_buffer->buffer[i].tlv, diff_buffer->buffer[i].data);
        current_position += sizeof(TLV);
    }

    return diff_buffer;
}
