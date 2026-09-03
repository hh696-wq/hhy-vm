#define _POSIX_C_SOURCE 200809L
#include "hhy/web_server.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif

#ifdef _WIN32

int hhy_web_server_run(const HhyWebServerOptions *options,
                       HhyWebServerHandler handler,
                       HhyWebServerStop should_stop,
                       void *context,
                       const char **error) {
    (void)options; (void)handler; (void)should_stop; (void)context;
    *error = "Web Server is not available in this Windows build";
    return 5;
}

#else

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

enum { HHY_WEB_READ_CHUNK = 8192 };

static const char *status_text(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 206: return "Partial Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 416: return "Range Not Satisfiable";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default: return "Response";
    }
}

static bool send_all(int socket_fd, const void *data, size_t length) {
    const unsigned char *cursor = data;
    while (length != 0) {
        ssize_t sent = send(socket_fd, cursor, length, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return false;
        cursor += (size_t)sent;
        length -= (size_t)sent;
    }
    return true;
}

static void send_simple(int socket_fd, int status, const char *message) {
    char header[512];
    size_t length = strlen(message);
    int count = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %zu\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\n\r\n",
        status, status_text(status), length);
    if (count > 0 && (size_t)count < sizeof(header)) {
        (void)send_all(socket_fd, header, (size_t)count);
        (void)send_all(socket_fd, message, length);
    }
}

static void discard_available_input(int socket_fd) {
    char buffer[4096];
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags >= 0) (void)fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    for (;;) {
        ssize_t received = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (received <= 0) break;
    }
    if (flags >= 0) (void)fcntl(socket_fd, F_SETFL, flags);
}

static char *find_header_end(char *buffer, size_t length) {
    if (length < 4) return NULL;
    for (size_t i = 0; i + 3 < length; i++)
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
            buffer[i + 2] == '\r' && buffer[i + 3] == '\n') return buffer + i;
    return NULL;
}

static bool valid_token(const char *text) {
    if (*text == '\0') return false;
    for (; *text != '\0'; text++) {
        unsigned char c = (unsigned char)*text;
        if (!(isalnum(c) || strchr("!#$%&'*+-.^_`|~", c) != NULL)) return false;
    }
    return true;
}

static void trim_value(char **value) {
    while (**value == ' ' || **value == '\t') (*value)++;
    size_t length = strlen(*value);
    while (length != 0 && ((*value)[length - 1] == ' ' || (*value)[length - 1] == '\t'))
        (*value)[--length] = '\0';
}

static bool parse_size(const char *text, size_t *value) {
    if (*text == '\0') return false;
    size_t result = 0;
    for (; *text != '\0'; text++) {
        if (!isdigit((unsigned char)*text)) return false;
        size_t digit = (size_t)(*text - '0');
        if (result > (SIZE_MAX - digit) / 10) return false;
        result = result * 10 + digit;
    }
    *value = result;
    return true;
}

static bool response_header_safe(const char *text) {
    return text != NULL && strchr(text, '\r') == NULL && strchr(text, '\n') == NULL;
}

static void write_response(int socket_fd, const HhyWebServerResponse *response) {
    int status = response->status >= 100 && response->status <= 599 ? response->status : 500;
    const char *content_type = response_header_safe(response->content_type)
        ? response->content_type : "application/octet-stream";
    char header[1024];
    int count = response->next_chunk == NULL
        ? snprintf(header, sizeof(header),
            "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
            "Connection: close\r\nX-Content-Type-Options: nosniff\r\n",
            status, status_text(status), content_type, response->body_length)
        : snprintf(header, sizeof(header),
            "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nTransfer-Encoding: chunked\r\n"
            "Connection: close\r\nX-Content-Type-Options: nosniff\r\n",
            status, status_text(status), content_type);
    if (count <= 0 || (size_t)count >= sizeof(header) ||
        !send_all(socket_fd, header, (size_t)count)) return;
    for (size_t i = 0; i < response->header_count; i++) {
        const HhyWebHeader *item = &response->headers[i];
        if (!response_header_safe(item->name) || !response_header_safe(item->value) ||
            !valid_token(item->name) || strcasecmp(item->name, "content-length") == 0 ||
            strcasecmp(item->name, "connection") == 0) continue;
        int line_count = snprintf(header, sizeof(header), "%s: %s\r\n", item->name, item->value);
        if (line_count <= 0 || (size_t)line_count >= sizeof(header) ||
            !send_all(socket_fd, header, (size_t)line_count)) return;
    }
    if (!send_all(socket_fd, "\r\n", 2)) return;
    if (response->next_chunk == NULL) {
        if (response->body_length != 0)
            (void)send_all(socket_fd, response->body, response->body_length);
        return;
    }
    bool sent = true;
    const unsigned char *chunk = NULL; size_t chunk_length = 0;
    while (response->next_chunk(response->stream_context, &chunk, &chunk_length)) {
        if (chunk_length == 0) continue;
        char size_line[32];
        int size_count = snprintf(size_line, sizeof(size_line), "%zx\r\n", chunk_length);
        if (size_count <= 0 || !send_all(socket_fd, size_line, (size_t)size_count) ||
            !send_all(socket_fd, chunk, chunk_length) || !send_all(socket_fd, "\r\n", 2)) {
            sent = false; break;
        }
    }
    if (sent) (void)send_all(socket_fd, "0\r\n\r\n", 5);
    if (response->close_stream != NULL) response->close_stream(response->stream_context);
}

static void handle_client(int socket_fd, const char *remote_address,
                          const HhyWebServerOptions *options,
                          HhyWebServerHandler handler, void *context) {
    size_t capacity = options->max_header_bytes + options->max_body_bytes + 4;
    char *buffer = malloc(capacity + 1);
    HhyWebHeader *headers = calloc(options->max_headers, sizeof(*headers));
    if (buffer == NULL || headers == NULL) {
        free(buffer); free(headers); send_simple(socket_fd, 503, "Service Unavailable\n"); return;
    }
    struct timeval timeout = {
        .tv_sec = (time_t)(options->idle_timeout_ms / 1000),
        .tv_usec = (suseconds_t)((options->idle_timeout_ms % 1000) * 1000)
    };
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    size_t used = 0, header_bytes = 0, body_length = 0;
    char *header_end = NULL;
    while (used < capacity) {
        size_t available = capacity - used;
        ssize_t received = recv(socket_fd, buffer + used,
                                available < HHY_WEB_READ_CHUNK ? available : HHY_WEB_READ_CHUNK, 0);
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) { send_simple(socket_fd, 408, "Request Timeout\n"); goto done; }
        used += (size_t)received; buffer[used] = '\0';
        header_end = find_header_end(buffer, used);
        if (header_end != NULL) { header_bytes = (size_t)(header_end - buffer) + 4; break; }
        if (used >= options->max_header_bytes) {
            send_simple(socket_fd, 413, "Request Headers Too Large\n"); goto done;
        }
    }
    if (header_end == NULL) { send_simple(socket_fd, 400, "Bad Request\n"); goto done; }
    *header_end = '\0';
    char *line_end = strstr(buffer, "\r\n");
    if (line_end == NULL) { send_simple(socket_fd, 400, "Bad Request\n"); goto done; }
    *line_end = '\0';
    char *method = buffer;
    char *target = strchr(method, ' ');
    if (target == NULL) { send_simple(socket_fd, 400, "Bad Request\n"); goto done; }
    *target++ = '\0';
    char *version = strchr(target, ' ');
    if (version == NULL) { send_simple(socket_fd, 400, "Bad Request\n"); goto done; }
    *version++ = '\0';
    if (!valid_token(method) || target[0] != '/' || strcmp(version, "HTTP/1.1") != 0) {
        send_simple(socket_fd, 400, "Bad Request\n"); goto done;
    }
    char *query = strchr(target, '?');
    if (query != NULL) *query++ = '\0';
    else query = target + strlen(target);

    size_t header_count = 0;
    char *cursor = line_end + 2;
    while (*cursor != '\0') {
        char *next = strstr(cursor, "\r\n");
        if (next != NULL) *next = '\0';
        char *colon = strchr(cursor, ':');
        if (colon == NULL || header_count >= options->max_headers) {
            send_simple(socket_fd, 400, "Bad Request\n"); goto done;
        }
        *colon = '\0';
        char *value = colon + 1; trim_value(&value);
        if (!valid_token(cursor)) { send_simple(socket_fd, 400, "Bad Request\n"); goto done; }
        headers[header_count++] = (HhyWebHeader){.name = cursor, .value = value};
        if (strcasecmp(cursor, "content-length") == 0 && !parse_size(value, &body_length)) {
            send_simple(socket_fd, 400, "Bad Request\n"); goto done;
        }
        if (strcasecmp(cursor, "transfer-encoding") == 0) {
            send_simple(socket_fd, 400, "Transfer-Encoding Is Not Supported\n"); goto done;
        }
        if (next == NULL) break;
        cursor = next + 2;
    }
    if (body_length > options->max_body_bytes) {
        send_simple(socket_fd, 413, "Payload Too Large\n");
        discard_available_input(socket_fd);
        goto done;
    }
    while (used - header_bytes < body_length) {
        ssize_t received = recv(socket_fd, buffer + used, capacity - used, 0);
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) { send_simple(socket_fd, 408, "Request Timeout\n"); goto done; }
        used += (size_t)received;
    }
    HhyWebServerRequest request = {
        .method = method, .target = target, .path = target, .query = query,
        .remote_address = remote_address, .headers = headers, .header_count = header_count,
        .body = (unsigned char *)buffer + header_bytes, .body_length = body_length
    };
    HhyWebServerResponse response = {0};
    if (!handler(context, &request, &response)) send_simple(socket_fd, 500, "Internal Server Error\n");
    else write_response(socket_fd, &response);
done:
    free(headers); free(buffer);
}

static int serve_listener(int listener, const HhyWebServerOptions *options,
                          HhyWebServerHandler handler, HhyWebServerStop should_stop,
                          void *context, const char **error) {
    while (should_stop == NULL || !should_stop(context)) {
        fd_set reads; FD_ZERO(&reads); FD_SET(listener, &reads);
        struct timeval wait = {.tv_sec = 0, .tv_usec = 200000};
        int ready = select(listener + 1, &reads, NULL, NULL, &wait);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) { *error = "Web Server accept loop failed"; return 5; }
        if (ready == 0) continue;
        struct sockaddr_in peer; socklen_t peer_length = sizeof(peer);
        int client = accept(listener, (struct sockaddr *)&peer, &peer_length);
        if (client < 0 && errno == EINTR) continue;
        if (client < 0) { *error = "Web Server accept failed"; return 5; }
        char remote[INET_ADDRSTRLEN] = "unknown";
        (void)inet_ntop(AF_INET, &peer.sin_addr, remote, sizeof(remote));
        handle_client(client, remote, options, handler, context);
        close(client);
    }
    return 0;
}

int hhy_web_server_run(const HhyWebServerOptions *options,
                       HhyWebServerHandler handler,
                       HhyWebServerStop should_stop,
                       void *context,
                       const char **error) {
    if (options == NULL || handler == NULL || options->host == NULL || options->port == 0 ||
        options->max_header_bytes < 1024 || options->max_headers == 0 ||
        options->max_body_bytes == 0 || options->idle_timeout_ms == 0 || options->workers == 0 ||
        options->workers > 64) {
        *error = "invalid Web Server options"; return 3;
    }
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { *error = "cannot create Web Server socket"; return 5; }
    int reuse = 1;
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET; address.sin_port = htons(options->port);
    if (inet_pton(AF_INET, options->host, &address.sin_addr) != 1) {
        close(listener); *error = "Web Server host must be an IPv4 address"; return 3;
    }
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 128) != 0) {
        close(listener); *error = "cannot bind or listen on Web Server address"; return 5;
    }
    if (options->workers == 1) {
        int result = serve_listener(listener, options, handler, should_stop, context, error);
        close(listener); return result;
    }
    pid_t *workers = calloc(options->workers, sizeof(*workers));
    if (workers == NULL) { close(listener); *error = "cannot allocate Web worker table"; return 5; }
    for (size_t i = 0; i < options->workers; i++) {
        workers[i] = fork();
        if (workers[i] == 0) {
            int result = serve_listener(listener, options, handler, should_stop, context, error);
            close(listener); _exit(result);
        }
        if (workers[i] < 0) {
            for (size_t j = 0; j < i; j++) kill(workers[j], SIGTERM);
            for (size_t j = 0; j < i; j++) (void)waitpid(workers[j], NULL, 0);
            free(workers); close(listener); *error = "cannot start Web worker process"; return 5;
        }
    }
    while (should_stop == NULL || !should_stop(context)) {
        for (size_t i = 0; i < options->workers; i++) {
            int status = 0;
            pid_t ended = waitpid(workers[i], &status, WNOHANG);
            if (ended == workers[i]) {
                workers[i] = fork();
                if (workers[i] == 0) {
                    int result = serve_listener(listener, options, handler, should_stop, context, error);
                    close(listener); _exit(result);
                }
                if (workers[i] < 0) { *error = "cannot restart Web worker process"; goto stop_workers; }
            }
        }
        struct timeval pause = {.tv_sec = 0, .tv_usec = 200000};
        (void)select(0, NULL, NULL, NULL, &pause);
    }
stop_workers:
    for (size_t i = 0; i < options->workers; i++) if (workers[i] > 0) kill(workers[i], SIGTERM);
    for (size_t i = 0; i < options->workers; i++) if (workers[i] > 0) (void)waitpid(workers[i], NULL, 0);
    free(workers);
    close(listener);
    return *error == NULL ? 0 : 5;
}

#endif
