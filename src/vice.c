/*
 * Some functions normally available in VICE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vice.h"


void *lib_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "fatal: failed to allocate %zu bytes.\n", size);
        exit(1);
    }
    return ptr;
}

void *lib_calloc(size_t nmemb, size_t size)
{
    void *ptr = calloc(nmemb, size);
    if (ptr == NULL) {
        fprintf(stderr, "fatal: failed to allocate %zu bytes\n", nmemb * size);
        exit(1);
    }
    return ptr;
}

void *lib_realloc(void *ptr, size_t size)
{
    void *tmp = realloc(ptr, size);
    if (tmp == NULL) {
        fprintf(stderr, "fatal: failed to reallocate %zu bytes\n", size);
        exit(1);
    }
    return tmp;
}

void lib_free(void *ptr)
{
    free(ptr);
}

char *lib_strdup(const char *s)
{
    size_t  len = strlen(s);
    char   *ptr = lib_malloc(len + 1u);
    memcpy(ptr, s, len + 1u);
    return ptr;
}
