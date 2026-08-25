#ifndef HHY_COMMON_H
#define HHY_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern bool hhy_suppress_diagnostics;

#ifndef HHY_VERSION
#define HHY_VERSION "unknown-dev"
#endif

typedef struct {
    const char *path;
    char *text;
    size_t length;
} HhySource;

bool hhy_source_load(const char *path, HhySource *out);
void hhy_source_free(HhySource *source);

void *hhy_alloc(size_t size);
void *hhy_realloc(void *pointer, size_t size);
char *hhy_strndup(const char *text, size_t length);

#endif
