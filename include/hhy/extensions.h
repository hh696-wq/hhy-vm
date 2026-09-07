#ifndef HHY_EXTENSIONS_H
#define HHY_EXTENSIONS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;

typedef struct {
    const char *kind;
    const char *code;
    const char *message;
    const char *operation;
    const char *stage;
    const char *cause;
} HhyExtensionError;

bool hhy_extension_prepare_namespace(const char *name, size_t length,
                                     const char **error);
bool hhy_extension_owns_callable(const char *name);
json_t *hhy_extension_call(const char *name, json_t *arguments,
                           HhyExtensionError *error);
json_t *hhy_extension_call_checked(const char *name, json_t *arguments,
                                    HhyExtensionError *error,
                                    bool (*cancelled)(void *), void *context);
void hhy_extensions_client_socket(int socket_fd);
unsigned long long hhy_extensions_scope_begin(void);
void hhy_extensions_scope_end(unsigned long long previous);
void hhy_extensions_shutdown(void);

#endif
