#ifndef ARR_H
#define ARR_H

#include <stdlib.h>

struct arr {
    size_t size;
    size_t capacity;
    char **items;
};

struct arr *initarr(size_t capacity);
int addarr(struct arr *arr, char *item);
void freearr(struct arr *arr);

#endif
