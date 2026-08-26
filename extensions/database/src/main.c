#define _POSIX_C_SOURCE 200809L
#include "database.h"

#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_MESSAGE = 1024 * 1024, MAX_ROWS = 10000 };

static bool emit(json_t *message) {
    char *text = json_dumps(message, JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS);
    if (text == NULL) return false;
    bool ok = puts(text) != EOF && fflush(stdout) == 0; free(text); return ok;
}

static json_t *read_message(void) {
    char *line = malloc(MAX_MESSAGE + 2); if (line == NULL) return NULL;
    if (fgets(line, MAX_MESSAGE + 2, stdin) == NULL) { free(line); return NULL; }
    size_t length = strlen(line); json_error_t error;
    json_t *value = length <= MAX_MESSAGE + 1
        ? json_loadb(line, length, JSON_REJECT_DUPLICATES, &error) : NULL;
    free(line); return json_is_object(value) ? value : NULL;
}

static json_t *envelope(const char *type, const char *request_id) {
    return json_pack("{s:s,s:s,s:s,s:s}", "type", type, "request_id", request_id,
                     "extension_id", "database", "protocol_version", "1.0");
}

static json_t *contract(const char *name, size_t minimum, size_t maximum,
                        const char *input, const char *output, bool action) {
    return json_pack("{s:s,s:i,s:i,s:s,s:s,s:s,s:b,s:b,s:b,s:b,s:s}",
        "name", name, "minimum_arity", (json_int_t)minimum, "maximum_arity", (json_int_t)maximum,
        "input", input, "output", output, "effect", "network", "lazy", 0,
        "cancel", 1, "sendable", 1, "action", action,
        "threading", "isolated_process");
}

static bool register_callables(void) {
    json_t *message = envelope("register", "register"), *items = json_array();
    json_array_append_new(items, contract("database.ping", 1, 1, "String", "Map", true));
    json_array_append_new(items, contract("database.query", 3, 4,
        "String, String, List<JsonScalar>, Int?", "Map", true));
    json_array_append_new(items, contract("database.execute", 3, 3,
        "String, String, List<JsonScalar>", "Map", true));
    json_array_append_new(items, contract("database.transaction", 2, 2,
        "String, List<Map>", "Map", true));
    json_object_set_new(message, "callables", items);
    bool ok = emit(message); json_decref(message); return ok;
}

static bool handle_call(json_t *request) {
    const char *id = json_string_value(json_object_get(request, "request_id"));
    const char *callable = json_string_value(json_object_get(request, "callable"));
    json_t *arguments = json_object_get(request, "arguments");
    if (id == NULL || callable == NULL || !json_is_array(arguments)) return false;
    const char *error = NULL; json_t *value = NULL;
    const char *url = json_string_value(json_array_get(arguments, 0));
    if (url == NULL) error = "first argument must be a database URL string";
    else if (strcmp(callable, "database.ping") == 0 && json_array_size(arguments) == 1)
        value = database_ping(url, &error);
    else if (strcmp(callable, "database.transaction") == 0 &&
             json_array_size(arguments) == 2) {
        json_t *statements = json_array_get(arguments, 1);
        if (!json_is_array(statements)) error = "transaction requires a statements list";
        else value = database_transaction(url, statements, &error);
    } else if ((strcmp(callable, "database.query") == 0 ||
              strcmp(callable, "database.execute") == 0) && json_array_size(arguments) >= 3) {
        const char *sql = json_string_value(json_array_get(arguments, 1));
        json_t *params = json_array_get(arguments, 2); size_t maximum = MAX_ROWS;
        if (json_array_size(arguments) == 4) {
            json_int_t requested = json_integer_value(json_array_get(arguments, 3));
            if (requested <= 0) error = "maximum rows must be a positive integer";
            else if ((size_t)requested < maximum) maximum = (size_t)requested;
        }
        if (error == NULL && (sql == NULL || !json_is_array(params)))
            error = "query requires SQL string and params list";
        else if (error == NULL && strcmp(callable, "database.query") == 0)
            value = database_query(url, sql, params, maximum, &error);
        else if (error == NULL && json_array_size(arguments) == 3)
            value = database_execute(url, sql, params, &error);
    } else error = "invalid database callable arguments";
    if (value == NULL) {
        json_t *response = envelope("error", id);
        json_object_set_new(response, "kind", json_string("DatabaseError"));
        json_object_set_new(response, "code", json_string("DATABASE_OPERATION_FAILED"));
        json_object_set_new(response, "message", json_string(error ? error : "database operation failed"));
        bool ok = emit(response); json_decref(response); return ok;
    }
    json_t *response = envelope("call_result", id); json_object_set_new(response, "value", value);
    bool ok = emit(response); json_decref(response); return ok;
}

int main(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[1], "--protocol") != 0 || strcmp(argv[2], "1") != 0) return 3;
    json_t *hello = read_message();
    if (hello == NULL || !json_is_string(json_object_get(hello, "runtime_version"))) {
        if (hello) json_decref(hello);
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
