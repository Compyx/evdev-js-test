#ifndef VICE_H
#define VICE_H

#include <stdlib.h>

void *lib_malloc(size_t size);
void *lib_calloc(size_t nmemb, size_t size);
void *lib_realloc(void *ptr, size_t size);
void  lib_free  (void *ptr);
char *lib_strdup(const char *s);

#endif
