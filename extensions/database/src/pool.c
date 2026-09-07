#define _POSIX_C_SOURCE 200809L
#include "database.h"
#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>

static DbSession sessions[DB_MAX_SESSIONS];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static uint64_t created, discarded, waits, timeouts, wait_ms, operations, errors, cancelled_count,
    operation_ms, returned_rows;
typedef struct {
    bool used;
    char request[96];
    atomic_bool cancelled;
} Operation;
static Operation operation_slots[8];
static _Thread_local Operation *operation;
void db_operation_begin(const char *request) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < 8; i++)
        if (!operation_slots[i].used) {
            operation = &operation_slots[i];
            operation->used = true;
            snprintf(operation->request, sizeof(operation->request), "%s", request);
            atomic_store(&operation->cancelled, false);
            break;
        }
    pthread_mutex_unlock(&mutex);
}
void db_operation_end(void) {
    pthread_mutex_lock(&mutex);
    if (operation)
        operation->used = false;
    operation = NULL;
    pthread_mutex_unlock(&mutex);
}
void db_record(uint64_t elapsed, const DbError *e, json_t *result) {
    pthread_mutex_lock(&mutex);
    operations++;
    operation_ms += elapsed;
    if (e->code[0]) {
        errors++;
        if (!strcmp(e->code, "DB_CANCELLED") || !strcmp(e->code, "DB_TIMEOUT"))
            cancelled_count++;
    }
    if (json_is_object(result))
        returned_rows += json_array_size(json_object_get(result, "rows"));
    pthread_mutex_unlock(&mutex);
}
uint64_t db_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}
void db_error(DbError *e, const char *code, const char *message) {
    snprintf(e->code, sizeof(e->code), "%s", code);
    e->message = message;
}
static bool text(json_t *v, char *out, size_t n) {
    if (!json_is_string(v) || json_string_length(v) >= n ||
        strlen(json_string_value(v)) != json_string_length(v))
        return false;
    memcpy(out, json_string_value(v), json_string_length(v) + 1);
    return true;
}
static bool number(json_t *obj, const char *key, unsigned *v, unsigned min, unsigned max) {
    json_t *x = json_object_get(obj, key);
    if (!x)
        return true;
    if (!json_is_integer(x) || json_integer_value(x) < min || json_integer_value(x) > max)
        return false;
    *v = (unsigned)json_integer_value(x);
    return true;
}
static bool curlpart(CURLU *u, CURLUPart p, char *out, size_t n, bool required) {
    char *s = NULL;
    CURLUcode r = curl_url_get(u, p, &s, CURLU_URLDECODE);
    if (r != CURLUE_OK)
        return !required;
    size_t len = strlen(s);
    bool ok = len < n;
    if (ok)
        memcpy(out, s, len + 1);
    curl_free(s);
    return ok;
}
bool db_config(json_t *input, DbConfig *c, DbError *e) {
    memset(c, 0, sizeof(*c));
    strcpy(c->driver, "mysql");
    strcpy(c->port, "3306");
    strcpy(c->tls, "verify_identity");
    c->max_open = 4;
    c->max_idle = 2;
    c->idle_ms = 60000;
    c->lifetime_ms = 1800000;
    c->connect_ms = 5000;
    c->acquire_ms = 1000;
    c->timeout_ms = 30000;
    bool ok = true, legacy = json_is_string(input);
    if (legacy) {
        if (strlen(json_string_value(input)) != json_string_length(input))
            goto invalid;
        CURLU *u = curl_url();
        if (!u)
            goto invalid;
        ok = curl_url_set(u, CURLUPART_URL, json_string_value(input), CURLU_NON_SUPPORT_SCHEME) ==
             CURLUE_OK;
        char path[512] = "";
        bool query = false;
        ok = ok && curlpart(u, CURLUPART_SCHEME, c->driver, sizeof(c->driver), true) &&
             curlpart(u, CURLUPART_HOST, c->host, sizeof(c->host), true) &&
             curlpart(u, CURLUPART_USER, c->user, sizeof(c->user), true) &&
             curlpart(u, CURLUPART_PASSWORD, c->password, sizeof(c->password), false) &&
             curlpart(u, CURLUPART_PATH, path, sizeof(path), true);
        if (!strcmp(c->driver, "postgres") || !strcmp(c->driver, "postgresql"))
            strcpy(c->port, "5432");
        ok = ok && curlpart(u, CURLUPART_PORT, c->port, sizeof(c->port), false);
        char *q = NULL;
        if (curl_url_get(u, CURLUPART_QUERY, &q, 0) == CURLUE_OK) {
            query = true;
            curl_free(q);
        }
        curl_url_cleanup(u);
        if (!ok || path[0] != '/' || strlen(path + 1) >= sizeof(c->database) ||
            strchr(path + 1, '/') || query)
            goto invalid;
        strcpy(c->database, path + 1);
        if (c->host[0] == '[') {
            size_t n = strlen(c->host);
            if (n < 3 || c->host[n - 1] != ']')
                goto invalid;
            memmove(c->host, c->host + 1, n - 2);
            c->host[n - 2] = 0;
        }
        bool local = !strcmp(c->host, "127.0.0.1") || !strcmp(c->host, "localhost") ||
                     !strcmp(c->host, "::1");
        if (!local) {
            db_error(e, "DB_PERMISSION",
                     "remote connections require an explicit data source configuration");
            return false;
        }
        strcpy(c->tls, "disabled");
    } else if (json_is_object(input)) {
        const char *key;
        json_t *v;
        json_object_foreach(input, key, v) {
            if (!strcmp(key, "driver"))
                ok = text(v, c->driver, sizeof(c->driver));
            else if (!strcmp(key, "host"))
                ok = text(v, c->host, sizeof(c->host));
            else if (!strcmp(key, "user"))
                ok = text(v, c->user, sizeof(c->user));
            else if (!strcmp(key, "password"))
                ok = text(v, c->password, sizeof(c->password));
            else if (!strcmp(key, "database"))
                ok = text(v, c->database, sizeof(c->database));
            else if (!strcmp(key, "tls"))
                ok = text(v, c->tls, sizeof(c->tls));
            else if (!strcmp(key, "ca"))
                ok = text(v, c->ca, sizeof(c->ca));
            else if (!strcmp(key, "port") || !strcmp(key, "allow") || !strcmp(key, "max_open") ||
                     !strcmp(key, "max_idle") || !strcmp(key, "idle_ms") ||
                     !strcmp(key, "lifetime_ms") || !strcmp(key, "connect_ms") ||
                     !strcmp(key, "acquire_ms") || !strcmp(key, "timeout_ms"))
                continue;
            else
                ok = false;
            if (!ok)
                goto invalid;
        }
        if (!strcmp(c->driver, "postgres") || !strcmp(c->driver, "postgresql"))
            strcpy(c->port, "5432");
        unsigned port = (unsigned)atoi(c->port);
        if (!number(input, "port", &port, 1, 65535))
            goto invalid;
        snprintf(c->port, sizeof(c->port), "%u", port);
        char endpoint[280];
        snprintf(endpoint, sizeof(endpoint), strchr(c->host, ':') ? "[%s]:%s" : "%s:%s", c->host,
                 c->port);
        json_t *allow = json_object_get(input, "allow");
        bool allowed = false;
        size_t i;
        json_t *v2;
        if (!json_is_array(allow) || json_array_size(allow) > 32)
            goto invalid;
        json_array_foreach(allow, i,
                           v2) if (json_is_string(v2) && !strcmp(json_string_value(v2), endpoint))
            allowed = true;
        if (!allowed) {
            db_error(e, "DB_PERMISSION", "data source endpoint is not explicitly authorized");
            return false;
        }
        if (!number(input, "max_open", &c->max_open, 1, 32) ||
            !number(input, "max_idle", &c->max_idle, 0, 32) ||
            !number(input, "idle_ms", &c->idle_ms, 1, 3600000) ||
            !number(input, "lifetime_ms", &c->lifetime_ms, 1, 86400000) ||
            !number(input, "connect_ms", &c->connect_ms, 100, 60000) ||
            !number(input, "acquire_ms", &c->acquire_ms, 0, 60000) ||
            !number(input, "timeout_ms", &c->timeout_ms, 100, 300000) || c->max_idle > c->max_open)
            goto invalid;
    } else
        goto invalid;
    if (!strcmp(c->driver, "postgres"))
        strcpy(c->driver, "postgresql");
    if (strcmp(c->driver, "mysql") && strcmp(c->driver, "postgresql"))
        goto invalid;
    if (!c->host[0] || !c->user[0] || !c->database[0] || strchr(c->host, '/') ||
        strchr(c->host, ' '))
        goto invalid;
    char *end;
    unsigned long port = strtoul(c->port, &end, 10);
    if (*end || port < 1 || port > 65535)
        goto invalid;
    if (strcmp(c->tls, "disabled") && strcmp(c->tls, "required") && strcmp(c->tls, "verify_ca") &&
        strcmp(c->tls, "verify_identity"))
        goto invalid;
    return true;
invalid:
    db_error(e, legacy ? "DATABASE_OPERATION_FAILED" : "DB_CONFIG",
             "invalid database configuration (use a structured map for connection options)");
    return false;
}
static void destroy(DbSession *s) {
    if (s->columns && !s->result_end)
        s->broken = true;
    if (s->broken) {
        if (s->my)
            shutdown((int)s->my->net.fd, SHUT_RDWR);
        if (s->pg)
            shutdown(PQsocket(s->pg), SHUT_RDWR);
    }
    db_result_close(s);
    free(s->prepared_sql);
    if (s->my)
        mysql_close(s->my);
    if (s->pg)
        PQfinish(s->pg);
    memset(s, 0, sizeof(*s));
    discarded++;
}
static void token(char out[65]) {
    unsigned char b[32];
    if (RAND_bytes(b, 32) != 1)
        abort();
    for (int i = 0; i < 32; i++)
        snprintf(out + 2 * i, 3, "%02x", b[i]);
}
DbSession *db_acquire(json_t *input, const char *scope, const char *request, DbError *e) {
    DbConfig config;
    char key[65];
    bool handle = json_is_string(input) && json_string_length(input) == 64;
    if (handle)
        for (size_t i = 0; i < 64; i++)
            if (!isxdigit((unsigned char)json_string_value(input)[i]))
                handle = false;
    if (!handle) {
        if (!db_config(input, &config, e))
            return NULL;
        unsigned char hash[32];
        unsigned len = 0;
        EVP_Digest(&config, sizeof(config), hash, &len, EVP_sha256(), NULL);
        for (int i = 0; i < 32; i++)
            snprintf(key + 2 * i, 3, "%02x", hash[i]);
    }
    uint64_t began = db_now(), until = began + (handle ? 0 : config.acquire_ms);
    bool counted = false;
    pthread_mutex_lock(&mutex);
    for (;;) {
        if (operation && atomic_load(&operation->cancelled)) {
            db_error(e, "DB_CANCELLED", "pool acquisition cancelled");
            goto fail;
        }
        DbSession *selected = NULL, *empty = NULL;
        unsigned count = 0;
        uint64_t now = db_now();
        for (int i = 0; i < DB_MAX_SESSIONS; i++) {
            DbSession *s = &sessions[i];
            if (s->used && !s->busy &&
                ((s->handle[0] && now >= s->resource_deadline) ||
                 (!s->handle[0] && (s->broken || now - s->touched > s->config.idle_ms ||
                                    now - s->born > s->config.lifetime_ms))))
                destroy(s);
            if (!s->used) {
                if (!empty)
                    empty = s;
                continue;
            }
            if (s->busy) {
                if (!handle && !strcmp(s->key, key))
                    count++;
                continue;
            }
            if (handle) {
                if (!strcmp(s->handle, json_string_value(input)) && !strcmp(s->scope, scope)) {
                    if (s->busy) {
                        db_error(e, "DB_BUSY", "resource is already in use");
                        goto fail;
                    }
                    selected = s;
                    break;
                }
            } else if (!strcmp(s->key, key)) {
                count++;
                if (!s->busy && !s->handle[0] && !selected)
                    selected = s;
            }
        }
        if (handle && !selected) {
            db_error(e, "DB_RESOURCE", "resource is closed, expired, or belongs to another scope");
            goto fail;
        }
        if (!selected && !handle && empty && count < config.max_open) {
            selected = empty;
            memset(selected, 0, sizeof(*selected));
            selected->used = true;
            selected->config = config;
            strcpy(selected->key, key);
            selected->born = now;
        }
        if (selected) {
            wait_ms += now - began;
            selected->busy = true;
            selected->deadline = now + selected->config.timeout_ms;
            if (selected->handle[0] && selected->resource_deadline < selected->deadline)
                selected->deadline = selected->resource_deadline;
            snprintf(selected->request, sizeof(selected->request), "%s", request);
            atomic_store(&selected->cancelled, false);
            if (!selected->handle[0])
                snprintf(selected->scope, sizeof(selected->scope), "%s", scope);
            pthread_mutex_unlock(&mutex);
            if (!selected->my && !selected->pg) {
                if (!db_connect(selected, e)) {
                    db_discard(selected);
                    return NULL;
                }
                pthread_mutex_lock(&mutex);
                created++;
                pthread_mutex_unlock(&mutex);
            }
            return selected;
        }
        if (now >= until) {
            timeouts++;
            db_error(e, "DB_POOL_TIMEOUT", "database pool acquisition timed out");
            goto fail;
        }
        if (!counted) {
            waits++;
            counted = true;
        }
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        t.tv_nsec += 10000000;
        if (t.tv_nsec >= 1000000000) {
            t.tv_sec++;
            t.tv_nsec -= 1000000000;
        }
        pthread_cond_timedwait(&changed, &mutex, &t);
    }
fail:
    pthread_mutex_unlock(&mutex);
    return NULL;
}
void db_pin(DbSession *s) {
    if (!s->handle[0]) {
        token(s->handle);
        s->resource_deadline =
            (s->transaction || s->columns) ? s->deadline : db_now() + s->config.lifetime_ms;
    }
}
void db_release(DbSession *s) {
    bool keep = s->handle[0] != 0;
    if (!keep && !s->broken && !db_reset(s))
        s->broken = true;
    pthread_mutex_lock(&mutex);
    unsigned idle = 0;
    for (int i = 0; i < DB_MAX_SESSIONS; i++)
        if (sessions[i].used && !sessions[i].busy && !strcmp(sessions[i].key, s->key))
            idle++;
    if (s->broken || (!keep && idle >= s->config.max_idle))
        destroy(s);
    else {
        s->busy = false;
        s->touched = db_now();
        s->request[0] = 0;
    }
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&mutex);
}
void db_discard(DbSession *s) {
    pthread_mutex_lock(&mutex);
    destroy(s);
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&mutex);
}
void db_scope_end(const char *scope) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < DB_MAX_SESSIONS; i++)
        if (sessions[i].used && !sessions[i].busy && sessions[i].handle[0] &&
            !strcmp(sessions[i].scope, scope))
            destroy(&sessions[i]);
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&mutex);
}
void db_cancel(const char *request) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < 8; i++)
        if (operation_slots[i].used && (!request || !strcmp(operation_slots[i].request, request)))
            atomic_store(&operation_slots[i].cancelled, true);
    for (int i = 0; i < DB_MAX_SESSIONS; i++)
        if (sessions[i].used && sessions[i].busy &&
            (!request || !strcmp(sessions[i].request, request)))
            atomic_store(&sessions[i].cancelled, true);
    pthread_mutex_unlock(&mutex);
}
json_t *db_stats(void) {
    pthread_mutex_lock(&mutex);
    unsigned open = 0, busy = 0, pinned = 0;
    for (int i = 0; i < DB_MAX_SESSIONS; i++)
        if (sessions[i].used) {
            open++;
            busy += sessions[i].busy;
            pinned += !sessions[i].busy && sessions[i].handle[0] != 0;
        }
    json_t *r =
        json_pack("{s:i,s:i,s:i,s:I,s:I,s:I,s:I}", "open", open, "in_use", busy, "pinned", pinned,
                  "created", (json_int_t)created, "discarded", (json_int_t)discarded, "wait_count",
                  (json_int_t)waits, "timeout_count", (json_int_t)timeouts);
    json_object_set_new(r, "wait_ms", json_integer((json_int_t)wait_ms));
    json_object_set_new(r, "operations", json_integer((json_int_t)operations));
    json_object_set_new(r, "errors", json_integer((json_int_t)errors));
    json_object_set_new(r, "cancelled", json_integer((json_int_t)cancelled_count));
    json_object_set_new(r, "operation_ms", json_integer((json_int_t)operation_ms));
    json_object_set_new(r, "returned_rows", json_integer((json_int_t)returned_rows));
    pthread_mutex_unlock(&mutex);
    return r;
}
void db_shutdown(void) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < DB_MAX_SESSIONS; i++)
        if (sessions[i].used)
            destroy(&sessions[i]);
    pthread_mutex_unlock(&mutex);
}
