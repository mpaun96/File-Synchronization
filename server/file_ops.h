#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "CRUDshared_array.h"
#include "glthread.h"
#include "diff.h"
#include <stddef.h>

extern int keycmp(void* T1, void* T2);

#define offset(struct_name, fld_name) \
    (size_t)&(((struct_name *)0)->fld_name)

extern glthread_t file_list;

void parseFile(const char *filename, glthread_t *glthread_head);
void apply_to_file_list(int i, char data[], glthread_t *glthread_head);
void write_glthread_to_file(const char *filename, glthread_t *glthread_head);

#endif