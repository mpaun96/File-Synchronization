#include "Server.h"
#include "CRUDshared_array.h"
#include "diff.h"
#include <stdatomic.h>
#include "glthread.h"
#include "send_client.h"


SharedArray shared_arr;
//atomic_int thread_running = 0;  // Atomic flag to track thread status
//atomic_int sync_ready = 0;  // Atomic flag to track thread status
glthread_t file_list;
client_list cl_list;
struct pollfd poll_fds[MAX_POLL_EVENTS];

int main(){

    //Initialize the shared array
    init_SharedArray(&shared_arr);
    init_glthread(&file_list);
    init_glthread(&cl_list.head);
    pthread_mutex_init(&cl_list.mutex, NULL);
    parseFile("test.txt", &file_list);

    start_server();


    pthread_mutex_destroy(&cl_list.mutex);
    delete_glthread_list(&file_list);
    delete_glthread_list(&cl_list.head);
    free_shared_array(&shared_arr);

    return 0;
}