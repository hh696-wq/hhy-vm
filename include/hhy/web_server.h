#ifndef HHY_WEB_SERVER_H
#define HHY_WEB_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    const char *value;
} HhyWebHeader;

typedef struct {
    const char *method;
    const char *target;
    const char *path;
    const char *query;
    const char *remote_address;
    const HhyWebHeader *headers;
    size_t header_count;
    const unsigned char *body;
    size_t body_length;
} HhyWebServerRequest;

typedef struct {
    int status;
    const char *content_type;
    const HhyWebHeader *headers;
    size_t header_count;
    const unsigned char *body;
    size_t body_length;
    bool (*next_chunk)(void *context, const unsigned char **data, size_t *length);
    void (*close_stream)(void *context);
    void *stream_context;
} HhyWebServerResponse;

typedef struct {
    const char *host;
    uint16_t port;
    size_t max_header_bytes;
    size_t max_headers;
    size_t max_body_bytes;
    uint64_t idle_timeout_ms;
    size_t workers;
} HhyWebServerOptions;

typedef bool (*HhyWebServerHandler)(void *context,
                                    const HhyWebServerRequest *request,
                                    HhyWebServerResponse *response);
typedef bool (*HhyWebServerStop)(void *context);

int hhy_web_server_run(const HhyWebServerOptions *options,
                       HhyWebServerHandler handler,
                       HhyWebServerStop should_stop,
                       void *context,
                       const char **error);

#endif
