#include "arr.h"

struct arr *
initarr(size_t capacity)
{
    struct arr *arr = malloc(sizeof(struct arr));
    if (arr == NULL) {
        return NULL;
    }

    arr->size = 0;
    arr->capacity = capacity;

    arr->items = calloc(capacity, sizeof(capacity));
    if (arr->items == NULL) {
        free(arr);
        return NULL;
    }

    return arr;
}

int
addarr(struct arr *arr, char *item)
{
    if (arr->size + 1 >= arr->capacity) {
        size_t capacity = arr->capacity * 2;
        char **items = realloc(arr->items, sizeof(char *) * capacity);
        if (items == NULL) {
            return 1;
        }

        arr->capacity = capacity;
        arr->items = items;
    }

    arr->items[arr->size++] = item;
    return 0;
}

void
freearr(struct arr *arr)
{
    for (size_t i = 0; i < arr->size; ++i) {
        free(arr->items[i]);
    }
    free(arr->items);
    free(arr);
}
