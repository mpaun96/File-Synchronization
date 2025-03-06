#include "CRUDshared_array.h"

//extern atomic_int thread_running;  // Atomic flag to track thread status
//extern atomic_int sync_ready;



// Function to initialize the SharedArray
void init_SharedArray(SharedArray* shared_array) {
    // Initialize the shared array
    for (int i = 0; i < TOTAL_TLVs; i++) {
        shared_array->shared_arr[i].array = NULL;   // No allocated memory initially
        shared_array->shared_arr[i].size = 0;       // No elements yet
        pthread_rwlock_init(&shared_array->shared_arr[i].small_lock, NULL); // Initialize small_lock
    }
    // Initialize the big_lock for the shared array
    pthread_rwlock_init(&shared_array->big_lock, NULL);
}

void free_shared_array(SharedArray* shared_array) {
    pthread_rwlock_wrlock(&shared_array->big_lock);  // Acquire write lock on the big lock

    for (int i = 0; i < TOTAL_TLVs; i++) {
        TLVArray* tlv_array = &shared_array->shared_arr[i];
        pthread_rwlock_wrlock(&tlv_array->small_lock); // Acquire write lock on the small lock

        if (tlv_array->array) {
            free(tlv_array->array);  // Free the dynamic array of t_TLVs
            tlv_array->array = NULL; // Avoid dangling pointers
            tlv_array->size = 0;     // Reset size
        }

        pthread_rwlock_unlock(&tlv_array->small_lock); // Release the small lock
    }

    pthread_rwlock_unlock(&shared_array->big_lock);  // Release the big lock
    pthread_rwlock_destroy(&shared_array->big_lock);  // Destroy the big lock
}

void write_shared_array(SharedArray* shared_array, DiffBuffer* deserialized_diff) {
    // Acquire a read lock on the big lock
    //printf("######################### CRUD update\n");
    pthread_rwlock_rdlock(&shared_array->big_lock);  // Acquire read lock for SharedArray

    for (int i = 0; i < deserialized_diff->size; i++) {
        TLV* tlv_entry = &deserialized_diff->buffer[i];
        int tlv_number = tlv_entry->tlv;

        TLVArray* tlv_array = &shared_array->shared_arr[tlv_number];  // Access the TLVArray directly

        // Release the read lock on the big lock before acquiring the small lock
        pthread_rwlock_unlock(&shared_array->big_lock);  // Release the read lock for SharedArray

        // Acquire a write lock on the small lock (TLVArray)
        pthread_rwlock_wrlock(&tlv_array->small_lock);  // Acquire write lock for specific TLVArray

        // Increment the size and reallocate memory for the new t_TLV entry
        tlv_array->size++;
        tlv_array->array = (t_TLV*)realloc(tlv_array->array, tlv_array->size * sizeof(t_TLV));

        // Copy data from TLV (deserialized) into t_TLV and append it to the shared array
        t_TLV new_tlv_entry;
        new_tlv_entry.tlv = tlv_entry->tlv;  // Copy the tlv number
        memcpy(new_tlv_entry.data, tlv_entry->data, MAX_LEN);  // Copy the data field

        // Use the last index to append to the shared_arr at the correct index
        int last_index = tlv_array->size - 1; // Get last index
        tlv_array->array[last_index] = new_tlv_entry;  // Copy the new t_TLV entry

        // Release the small lock after modification
        pthread_rwlock_unlock(&tlv_array->small_lock);  // Release the write lock for specific TLVArray

        // Reacquire the big lock for the next iteration
        pthread_rwlock_rdlock(&shared_array->big_lock);  // Reacquire read lock for SharedArray
    }

    // Finally, release the big lock after all modifications are done
    pthread_rwlock_unlock(&shared_array->big_lock);  // Release the read lock for SharedArray
}

void print_shared_arr(SharedArray* shared_array) {
    // Acquire a read lock on the big lock
    pthread_rwlock_rdlock(&shared_array->big_lock);  // Acquire read lock for SharedArray

    for (int i = 0; i < TOTAL_TLVs; i++) {
        TLVArray* tlv_array = &shared_array->shared_arr[i];

        // Acquire a read lock on the small lock (TLVArray) before accessing its data
        pthread_rwlock_rdlock(&tlv_array->small_lock);  // Acquire read lock for specific TLVArray

        if (tlv_array->size == 0) {
            pthread_rwlock_unlock(&tlv_array->small_lock);  // Release the small lock if the array is empty
            continue;  // Skip empty arrays
        }
        printf("TLV: %d:\n", i);  // Print TLV number

        for (int j = 0; j < tlv_array->size; j++) {
            printf("%d: %s\n", j + 1, tlv_array->array[j].data);  // Print each data item in the TLV array
        }
        // Release the small lock after finishing with this TLVArray
        pthread_rwlock_unlock(&tlv_array->small_lock);  // Release the read lock for specific TLVArray
    }
    // Finally, release the big lock after printing all TLVArrays
    pthread_rwlock_unlock(&shared_array->big_lock);  // Release the read lock for SharedArray
}



void free_TLVArray(TLVArray* tlv_array) {
    // Acquire a write lock on the small lock (TLVArray) to ensure exclusive access
    pthread_rwlock_wrlock(&tlv_array->small_lock);  // Acquire write lock for TLVArray

    if (tlv_array->array) {
        free(tlv_array->array);     // Free the dynamic array of t_TLVs
        tlv_array->array = NULL;    // Avoid dangling pointer
        tlv_array->size = 0;        // Reset size
    }

    // Release the small lock after freeing the array
    pthread_rwlock_unlock(&tlv_array->small_lock);  // Release write lock for TLVArray
}

int is_shared_array_empty(SharedArray* shared_array) {
    // Acquire a read lock on the big lock (SharedArray) to check its contents
    pthread_rwlock_rdlock(&shared_array->big_lock);  // Acquire read lock for SharedArray

    for (int i = 0; i < TOTAL_TLVs; i++) {
        TLVArray* tlv_array = &shared_array->shared_arr[i];

        // Acquire a read lock on the small lock (TLVArray) before checking its state
        pthread_rwlock_rdlock(&tlv_array->small_lock);  // Acquire read lock for specific TLVArray

        // Check if array is NULL and size is 0
        if (tlv_array->array != NULL || tlv_array->size != 0) {
            pthread_rwlock_unlock(&tlv_array->small_lock);  // Release small lock before returning
            pthread_rwlock_unlock(&shared_array->big_lock);  // Release big lock
            return 0;  // Return 0 (false) if any TLVArray is not NULL or size is not 0
        }

        pthread_rwlock_unlock(&tlv_array->small_lock);  // Release small lock after checking
    }

    // Release the big lock after checking all TLVArrays
    pthread_rwlock_unlock(&shared_array->big_lock);  // Release read lock for SharedArray
    return 1;  // Return 1 (true) if all TLVArrays are NULL and size is 0
}

// Function to read and append the shared array contents
void* process_shared_arr(void* arg) {

    SharedArray* shared_array = (SharedArray*)arg;

    printf("-----------> Content of the sharred array(all the changes sent by all clients)\n");
    print_shared_arr(shared_array);

    // Acquire a write lock for the shared array
    pthread_rwlock_wrlock(&shared_array->big_lock);  // Acquire write lock on the SharedArray
    for (int i = 0; i < TOTAL_TLVs; i++) {
        TLVArray* tlv_array = &shared_array->shared_arr[i];

        if (shared_array->shared_arr[i].array == NULL || shared_array->shared_arr[i].size == 0) {
            continue;  // Skip empty arrays
        }

        printf("\n-----------> For TLV %d:\n", i);  // Print TLV number

        for (int j = 0; j < tlv_array->size; j++) {
            printf("%d: %s\n", j, tlv_array->array[j].data);  // Print each data item in the TLV array
        }
        printf("Type a number between 0-%d to make the choice\n", tlv_array->size -1);

        int upperlim = tlv_array->size - 1;
        int choice;
        char input_buffer[100]; // Buffer to store input

        // Inner loop for user input for the current TLV
        do {
            printf("Enter (0-%d): ", upperlim);
            
            // Use fgets to read the input as a string
            if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
                //printf("got past fgets\n");
                // Try to convert input to an integer using sscanf
                if (sscanf(input_buffer, "%d", &choice) == 1) {
                    //printf("got past sscanf\n");
                    // Check if the input is within valid bounds
                    if (choice >= 0 && choice <= upperlim) {
                        printf("Chose to append: %s\n", tlv_array->array[choice].data);
                        
                        // Apply the choice to the file list 
                        apply_to_file_list(i, tlv_array->array[choice].data, &file_list);
                        free_TLVArray(&shared_array->shared_arr[i]);
                        break; // Valid choice made, exit the inner loop
                    } else {
                        printf("Not a valid choice, must be between 0 and %d\n", upperlim);
                    }
                } else {
                    // If sscanf fails, it's not a valid number
                    printf("Invalid input. Please enter a number between 0 and %d\n", upperlim);
                }
            }
            // Clear leftover input in stdin if needed
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF); // Clear stdin buffer
        } while (1); // Continue until a valid choice is made

    }
    // Release the write lock on the shared array
    pthread_rwlock_unlock(&shared_array->big_lock);  // Release the write lock

    if(is_shared_array_empty(shared_array) == 1) {
        printf("-----------> ALL THE UPDATES HAVE BEEN PROCESSED!\n");

        //Turn back on pollin on stdin
        poll_fds[1].events = POLLIN;
        
        printf("\n-----------> Content of file_list before being written to the file:\n");
        glthread_t *curr = NULL;
        ITERATE_GLTHREAD_BEGIN(&file_list, curr){
            TLV *p = thread_TLV(curr);
            printf("TLV = %d; data = %s\n", p->tlv, p->data);
        } ITERATE_GLTHREAD_END(&base_glthread, curr);

        //Locally modify our file
        write_glthread_to_file("test.txt",&file_list);

        //printf("CALLING THE SYNC FUNCTION....\n");
        sync_function();
    }
    printf("#########################################################END process_shared_array, mutex unlocked\n");

    return NULL;
}