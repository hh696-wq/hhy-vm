#define _POSIX_C_SOURCE 200809L
#include "database.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

enum { MAX_MESSAGE = 1024 * 1024, THREADS = 8, QUEUE = 64 };
static pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER,
                       queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work = PTHREAD_COND_INITIALIZER;
static json_t *queue[QUEUE];
static unsigned head, tail, queued, active;
static bool stopping;
static json_t *envelope(const char *type, const char *id) {
    return json_pack("{s:s,s:s,s:s,s:s}", "type", type, "request_id", id, "extension_id",
                     "database", "protocol_version", "1.0");
}
static bool emit(json_t *m) {
    char *s = json_dumps(m, JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS);
    if (!s)
        return false;
    bool ok = strlen(s) <= MAX_MESSAGE;
    pthread_mutex_lock(&output_mutex);
    if (ok)
        ok = puts(s) != EOF && fflush(stdout) == 0;
    pthread_mutex_unlock(&output_mutex);
    free(s);
    return ok;
}
static json_t *read_message(void) {
    char *line = malloc(MAX_MESSAGE + 2);
    if (!line)
        return NULL;
    if (!fgets(line, MAX_MESSAGE + 2, stdin)) {
        free(line);
        return NULL;
    }
    size_t n = strlen(line);
    json_error_t e;
    json_t *v = NULL;
    if (n <= MAX_MESSAGE + 1 && n && line[n - 1] == '\n')
        v = json_loadb(line, n, JSON_REJECT_DUPLICATES, &e);
    free(line);
    if (!json_is_object(v)) {
        json_decref(v);
        return NULL;
    }
    return v;
}
static bool identity(json_t *m) {
    const char *id = json_string_value(json_object_get(m, "request_id")),
               *ext = json_string_value(json_object_get(m, "extension_id")),
               *v = json_string_value(json_object_get(m, "protocol_version"));
    return id && strlen(id) < 96 && ext && !strcmp(ext, "database") && v && !strcmp(v, "1.0");
}
static void error_response(const char *id, DbError *e) {
    json_t *m = envelope("error", id);
    json_object_set_new(m, "kind", json_string("DatabaseError"));
    json_object_set_new(m, "code", json_string(e->code[0] ? e->code : "DB_INTERNAL"));
    json_object_set_new(m, "message",
                        json_string(e->message ? e->message : "database operation failed"));
    if (e->state[0])
        json_object_set_new(m, "cause", json_string(e->state));
    json_object_set_new(m, "stage", json_string("database"));
    json_object_set_new(m, "vendor_code", json_integer(e->vendor));
    (void)emit(m);
    json_decref(m);
}
static void *worker(void *unused) {
    (void)unused;
    mysql_thread_init();
    for (;;) {
        pthread_mutex_lock(&queue_mutex);
        while (!queued && !stopping)
            pthread_cond_wait(&work, &queue_mutex);
        if (!queued && stopping) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        json_t *m = queue[head];
        head = (head + 1) % QUEUE;
        queued--;
        active++;
        pthread_mutex_unlock(&queue_mutex);
        const char *id = json_string_value(json_object_get(m, "request_id")),
                   *name = json_string_value(json_object_get(m, "callable")),
                   *scope = json_string_value(json_object_get(m, "scope"));
        DbError e = {0};
        json_t *value = NULL;
        if (!scope)
            scope = "legacy";
        uint64_t began = db_now();
        db_operation_begin(id);
        if (json_is_true(json_object_get(m, "_cancelled")))
            db_error(&e, "DB_CANCELLED", "queued database operation cancelled");
        else if (!name || strlen(scope) >= 96)
            db_error(&e, "DB_ARGUMENT", "invalid callable or request scope");
        else
            value = db_dispatch(name, json_object_get(m, "arguments"), scope, id, &e);
        db_operation_end();
        db_record(db_now() - began, &e, value);
        if (value) {
            json_t *r = envelope("call_result", id);
            json_object_set_new(r, "value", value);
            if (!emit(r)) {
                db_error(&e, "DB_LIMIT", "response exceeds protocol limit");
                error_response(id, &e);
            }
            json_decref(r);
        } else
            error_response(id, &e);
        json_decref(m);
        pthread_mutex_lock(&queue_mutex);
        active--;
        pthread_cond_broadcast(&work);
        pthread_mutex_unlock(&queue_mutex);
    }
    mysql_thread_end();
    return NULL;
}
static bool register_callables(void) {
    struct {
        const char *name;
        int min, max;
    } defs[] = {{"connection", 1, 1},
                {"ping", 1, 1},
                {"query", 3, 4},
                {"execute", 3, 3},
                {"transaction", 2, 2},
                {"begin", 1, 2},
                {"commit", 1, 1},
                {"rollback", 1, 1},
                {"savepoint", 2, 2},
                {"rollback_to", 2, 2},
                {"release_savepoint", 2, 2},
                {"cursor", 3, 4},
                {"fetch", 1, 2},
                {"next_result", 1, 1},
                {"close", 1, 1},
                {"close_result", 1, 1},
                {"execute_many", 3, 3},
                {"prepare", 2, 2},
                {"execute_prepared", 2, 3},
                {"query_prepared", 2, 3},
                {"stats", 0, 0},
                {"capabilities", 0, 0},
                {"with_transaction", 2, 3},
                {"stream", 3, 4}};
    json_t *m = envelope("register", "register"), *items = json_array();
    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++) {
        char name[64];
        snprintf(name, sizeof(name), "database.%s", defs[i].name);
        json_t *c = json_pack("{s:s,s:i,s:i,s:s,s:s,s:s,s:b,s:b,s:b,s:b,s:s}", "name", name,
                              "minimum_arity", defs[i].min, "maximum_arity", defs[i].max, "input",
                              "Value", "output", "Value", "effect", "network", "lazy", 0, "cancel",
                              1, "sendable", 0, "action", 1, "threading", "isolated_process");
        json_array_append_new(items, c);
    }
    json_object_set_new(m, "callables", items);
    bool ok = emit(m);
    json_decref(m);
    return ok;
}
int main(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[1], "--protocol") || strcmp(argv[2], "1"))
        return 3;
    signal(SIGPIPE, SIG_IGN);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    mysql_library_init(0, NULL, NULL);
    json_t *hello = read_message();
    if (!hello || !identity(hello) || !json_is_string(json_object_get(hello, "type")) ||
        strcmp(json_string_value(json_object_get(hello, "type")), "handshake") ||
        strcmp(json_string_value(json_object_get(hello, "request_id")), "handshake") ||
        !json_is_string(json_object_get(hello, "runtime_version"))) {
        json_decref(hello);
        return 2;
    }
    json_decref(hello);
    json_t *response = envelope("handshake_result", "handshake");
    json_object_set_new(response, "extension_version", json_string(DB_VERSION));
    json_object_set_new(response, "scoped_resources", json_true());
    bool ok = emit(response);
    json_decref(response);
    if (!ok || !register_callables())
        return 2;
    pthread_t threads[THREADS];
    unsigned started = 0;
    for (; started < THREADS; started++)
        if (pthread_create(&threads[started], NULL, worker, NULL) != 0)
            break;
    if (started != THREADS)
        return 2;
    for (;;) {
        json_t *m = read_message();
        if (!m)
            break;
        if (!identity(m)) {
            json_decref(m);
            break;
        }
        const char *type = json_string_value(json_object_get(m, "type")),
                   *id = json_string_value(json_object_get(m, "request_id"));
        if (type && !strcmp(type, "shutdown")) {
            json_decref(m);
            break;
        }
        if (type && !strcmp(type, "cancel")) {
            const char *target = json_string_value(json_object_get(m, "target"));
            if (target) {
                db_cancel(target);
                pthread_mutex_lock(&queue_mutex);
                for (unsigned i = 0; i < queued; i++) {
                    json_t *item = queue[(head + i) % QUEUE];
                    const char *queued_id = json_string_value(json_object_get(item, "request_id"));
                    if (queued_id && !strcmp(queued_id, target))
                        json_object_set_new(item, "_cancelled", json_true());
                }
                pthread_mutex_unlock(&queue_mutex);
            }
            json_decref(m);
            continue;
        }
        if (type && !strcmp(type, "scope_end")) {
            const char *scope = json_string_value(json_object_get(m, "scope"));
            /* Host sends this after consuming every response in that scope. */
            if (scope)
                db_scope_end(scope);
            json_t *r = envelope("scope_ended", id);
            emit(r);
            json_decref(r);
            json_decref(m);
            continue;
        }
        if (!type || strcmp(type, "call")) {
            json_decref(m);
            break;
        }
        pthread_mutex_lock(&queue_mutex);
        if (queued == QUEUE) {
            pthread_mutex_unlock(&queue_mutex);
            DbError e = {0};
            db_error(&e, "DB_QUEUE_FULL", "database request queue is full");
            error_response(id, &e);
            json_decref(m);
            continue;
        }
        queue[tail] = m;
        tail = (tail + 1) % QUEUE;
        queued++;
        pthread_cond_signal(&work);
        pthread_mutex_unlock(&queue_mutex);
    }
    db_cancel(NULL);
    pthread_mutex_lock(&queue_mutex);
    for (unsigned i = 0; i < queued; i++)
        json_object_set_new(queue[(head + i) % QUEUE], "_cancelled", json_true());
    stopping = true;
    pthread_cond_broadcast(&work);
    pthread_mutex_unlock(&queue_mutex);
    for (unsigned i = 0; i < started; i++)
        pthread_join(threads[i], NULL);
    db_shutdown();
    mysql_library_end();
    curl_global_cleanup();
    return 0;
}
