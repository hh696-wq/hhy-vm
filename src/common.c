#include "hhy/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool hhy_suppress_diagnostics = false;

void *hhy_alloc(size_t size) {
    void *result = calloc(1, size);
    if (result == NULL) {
        fputs("hhy: out of memory\n", stderr);
        exit(70);
    }
    return result;
}

void *hhy_realloc(void *pointer, size_t size) {
    void *result = realloc(pointer, size);
    if (result == NULL && size != 0) {
        fputs("hhy: out of memory\n", stderr);
        exit(70);
    }
    return result;
}

char *hhy_strndup(const char *text, size_t length) {
    char *result = hhy_alloc(length + 1);
    memcpy(result, text, length);
    result[length] = '\0';
    return result;
}

bool hhy_source_load(const char *path, HhySource *out) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "hhy: cannot open %s\n", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    char *text = hhy_alloc((size_t)size + 1);
    size_t read = fread(text, 1, (size_t)size, file);
    if (read != (size_t)size || ferror(file)) {
        fprintf(stderr, "hhy: cannot read %s\n", path);
        free(text);
        fclose(file);
        return false;
    }
    fclose(file);
    text[read] = '\0';
    out->path = path;
    out->text = text;
    out->length = read;
    return true;
}

void hhy_source_free(HhySource *source) {
    free(source->text);
    source->text = NULL;
    source->length = 0;
}
