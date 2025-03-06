#include "file_ops.h"


void parseFile(const char *filename, glthread_t *glthread_head) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Could not open file %s\n", filename);
        //exit(1);
        return;
    }

    char line[MAX_LEN];
    while (fgets(line, sizeof(line), file)) {
        int tlv;
        char data[MAX_LEN];

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

void apply_to_file_list(int i, char data[], glthread_t *glthread_head) {

    int found = 0;

    glthread_t *curr = NULL;
    ITERATE_GLTHREAD_BEGIN(glthread_head, curr){
        TLV *p = thread_TLV(curr);
        //printf("TLV = %d; data = %s\n", p->tlv, p->data);
        if(p->tlv == i){
            found = 1;
            if(strcmp(data, "DELETE") == 0){
                remove_glthread(curr);
                break;
            }else if(data){
                strcpy(p->data, data);
                break;
            }
        }
    } ITERATE_GLTHREAD_END(&base_glthread, curr);

    if(found == 0 && !(strcmp(data, "DELETE") == 0)){
        TLV* node = (TLV*)calloc(1,sizeof(TLV));
        node->tlv = i;
        strcpy(node->data, data);
        glthread_priority_insert(glthread_head, &node->glthread, keycmp, offset(TLV, glthread));
    }
    //if(found == 0 && (strcmp(data, "DELETE") == 0)){ do nothing ...}
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
