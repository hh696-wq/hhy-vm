#define _POSIX_C_SOURCE 200809L
#include <jansson.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { MAX_MESSAGE = 1024 * 1024 };

static bool emit(json_t *message) {
    char *text = json_dumps(message, JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS);
    if (text == NULL) return false;
    bool ok = puts(text) != EOF && fflush(stdout) == 0; free(text); return ok;
}

static json_t *read_message(void) {
    char *line = malloc(MAX_MESSAGE + 2);
    if (line == NULL) return NULL;
    if (fgets(line, MAX_MESSAGE + 2, stdin) == NULL) { free(line); return NULL; }
    size_t length = strlen(line); json_error_t error;
    json_t *message = length <= MAX_MESSAGE + 1
        ? json_loadb(line, length, JSON_REJECT_DUPLICATES, &error) : NULL;
    free(line); return json_is_object(message) ? message : NULL;
}

static json_t *envelope(const char *type, const char *request_id) {
    return json_pack("{s:s,s:s,s:s,s:s}", "type", type, "request_id", request_id,
                     "extension_id", "sample", "protocol_version", "1.0");
}

static bool register_callables(void) {
    json_t *message = envelope("register", "register"); json_t *items = json_array();
    json_array_append_new(items, json_pack("{s:s,s:i,s:i,s:s,s:s,s:s,s:b,s:b,s:b,s:b}",
        "name", "sample.echo", "minimum_arity", 1, "maximum_arity", 1,
        "input", "Value", "output", "Value", "effect", "none", "lazy", 0,
        "cancel", 0, "sendable", 1, "action", 0));
    json_object_set_new(json_array_get(items, 0), "threading", json_string("isolated_process"));
    json_array_append_new(items, json_pack("{s:s,s:i,s:i,s:s,s:s,s:s,s:b,s:b,s:b,s:b}",
        "name", "sample.fail", "minimum_arity", 0, "maximum_arity", 0,
        "input", "()", "output", "Never", "effect", "custom", "lazy", 0,
        "cancel", 0, "sendable", 1, "action", 1));
    json_object_set_new(json_array_get(items, 1), "threading", json_string("isolated_process"));
    json_array_append_new(items, json_pack("{s:s,s:i,s:i,s:s,s:s,s:s,s:b,s:b,s:b,s:b}",
        "name", "sample.wait", "minimum_arity", 1, "maximum_arity", 1,
        "input", "Int", "output", "Int", "effect", "none", "lazy", 0,
        "cancel", 1, "sendable", 1, "action", 0));
    json_object_set_new(json_array_get(items, 2), "threading", json_string("isolated_process"));
    json_object_set_new(message, "callables", items);
    bool ok = emit(message); json_decref(message); return ok;
}

static bool handle_call(json_t *request) {
    const char *request_id = json_string_value(json_object_get(request, "request_id"));
    const char *callable = json_string_value(json_object_get(request, "callable"));
    json_t *arguments = json_object_get(request, "arguments");
    if (request_id == NULL || callable == NULL || !json_is_array(arguments)) return false;
    if (strcmp(callable, "sample.fail") == 0) {
        json_t *error = envelope("error", request_id);
        json_object_set_new(error, "kind", json_string("SampleError"));
        json_object_set_new(error, "code", json_string("SAMPLE_FAILURE"));
        json_object_set_new(error, "message", json_string("sample failure"));
        bool ok = emit(error); json_decref(error); return ok;
    }
    json_t *value = NULL;
    if (strcmp(callable, "sample.echo") == 0 && json_array_size(arguments) == 1)
        value = json_array_get(arguments, 0);
    else if (strcmp(callable, "sample.wait") == 0 && json_array_size(arguments) == 1 &&
             json_is_integer(json_array_get(arguments, 0))) {
        json_int_t milliseconds = json_integer_value(json_array_get(arguments, 0));
        if (milliseconds < 0 || milliseconds > 5000) return false;
        struct timespec delay = {.tv_sec = (time_t)(milliseconds / 1000),
            .tv_nsec = (long)(milliseconds % 1000) * 1000000L};
        while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
        value = json_array_get(arguments, 0);
    }
    if (value == NULL) return false;
    json_t *response = envelope("call_result", request_id);
    json_object_set(response, "value", value);
    bool ok = emit(response); json_decref(response); return ok;
}

int main(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[1], "--protocol") != 0 || strcmp(argv[2], "1") != 0) return 3;
    json_t *hello = read_message();
    if (hello == NULL || strcmp(json_string_value(json_object_get(hello, "type")), "handshake") != 0) {
        if (hello != NULL) json_decref(hello);
        return 2;
    }
    json_decref(hello); json_t *response = envelope("handshake_result", "handshake");
    json_object_set_new(response, "extension_version", json_string("0.1.0"));
    bool ok = emit(response); json_decref(response);
    if (!ok || !register_callables()) return 2;
    for (;;) {
        json_t *request = read_message(); if (request == NULL) break;
        const char *type = json_string_value(json_object_get(request, "type"));
        if (type != NULL && strcmp(type, "shutdown") == 0) { json_decref(request); return 0; }
        ok = type != NULL && strcmp(type, "call") == 0 && handle_call(request);
        json_decref(request); if (!ok) return 2;
    }
    return 0;
}
