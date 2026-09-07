#define _POSIX_C_SOURCE 200809L
#include "database.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

typedef __typeof__(*(((MYSQL_BIND *)0)->is_null)) DbBool;
void db_driver_error(DbSession *s, DbError *e, const char *stage) {
    const char *state = "HY000";
    unsigned code = 0;
    if (s->my) {
        state = s->stmt ? mysql_stmt_sqlstate(s->stmt) : mysql_sqlstate(s->my);
        code = s->stmt ? mysql_stmt_errno(s->stmt) : mysql_errno(s->my);
    } else if (s->pg_pending) {
        const char *p = PQresultErrorField(s->pg_pending, PG_DIAG_SQLSTATE);
        if (p)
            state = p;
    }
    snprintf(e->state, sizeof(e->state), "%.5s", state);
    e->vendor = code;
    const char *kind = "DB_SQL";
    if (!strncmp(state, "23", 2))
        kind = "DB_CONSTRAINT";
    else if (!strncmp(state, "40", 2) || code == 1213)
        kind = "DB_RETRY_TRANSACTION";
    else if (!strncmp(state, "28", 2) || code == 1045)
        kind = "DB_AUTH";
    else if (!strncmp(state, "08", 2) || code == 2006 || code == 2013 ||
             (s->pg && PQstatus(s->pg) != CONNECTION_OK)) {
        kind = "DB_CONNECTION_LOST";
        s->broken = true;
    } else if (!strcmp(state, "57014") || code == 3024 || code == 1317 || code == 1205)
        kind = "DB_TIMEOUT";
    else if (code == 2026)
        kind = "DB_TLS";
    db_error(e, kind, stage);
}
static bool ready(DbSession *s, int fd, short events, DbError *e) {
    while (db_now() < s->deadline && !atomic_load(&s->cancelled)) {
        struct pollfd p = {fd, events, 0};
        int r = poll(&p, 1, 25);
        if (r > 0) {
            if (p.revents & (POLLERR | POLLNVAL)) {
                s->broken = true;
                db_error(e, "DB_CONNECTION_LOST", "database socket failed");
                return false;
            }
            return true;
        }
        if (r < 0 && errno != EINTR)
            break;
    }
    db_error(e, atomic_load(&s->cancelled) ? "DB_CANCELLED" : "DB_TIMEOUT",
             "database operation cancelled or deadline exceeded");
    if (s->pg) {
#ifdef LIBPQ_HAS_ASYNC_CANCEL
        PGcancelConn *cancel = PQcancelCreate(s->pg);
        if (cancel && PQcancelStart(cancel)) {
            uint64_t until = db_now() + 1000;
            while (db_now() < until) {
                PostgresPollingStatusType status = PQcancelPoll(cancel);
                if (status == PGRES_POLLING_OK || status == PGRES_POLLING_FAILED)
                    break;
                struct pollfd p = {PQcancelSocket(cancel),
                                   status == PGRES_POLLING_WRITING ? POLLOUT : POLLIN, 0};
                (void)poll(&p, 1, 20);
            }
        }
        if (cancel)
            PQcancelFinish(cancel);
#else
        PGcancel *cancel = PQgetCancel(s->pg);
        if (cancel) {
            char ignored[256];
            (void)PQcancel(cancel, ignored, sizeof(ignored));
            PQfreeCancel(cancel);
        }
#endif
    }
    s->broken = true;
    return false;
}
static PGresult *pg_result(DbSession *s, DbError *e) {
    for (;;) {
        int f = PQflush(s->pg);
        if (f == 0)
            break;
        if (f < 0 || !ready(s, PQsocket(s->pg), POLLOUT, e))
            return NULL;
    }
    while (PQisBusy(s->pg)) {
        if (!ready(s, PQsocket(s->pg), POLLIN, e) || !PQconsumeInput(s->pg))
            return NULL;
    }
    return PQgetResult(s->pg);
}
/* A dedicated control connection cancels only this session's current query. */
typedef struct {
    DbSession *s;
    unsigned long id;
    atomic_bool done, expired;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
} Guard;
static void *watch(void *ptr) {
    Guard *g = ptr;
    pthread_mutex_lock(&g->mutex);
    while (!atomic_load(&g->done)) {
        if (db_now() >= g->s->deadline || atomic_load(&g->s->cancelled)) {
            atomic_store(&g->expired, true);
            pthread_mutex_unlock(&g->mutex);
            mysql_thread_init();
            DbSession ctl = {0};
            ctl.config = g->s->config;
            ctl.config.connect_ms = 1000;
            ctl.config.timeout_ms = 1000;
            ctl.deadline = db_now() + 1000;
            DbError e = {0};
            if (db_connect(&ctl, &e)) {
                char query[80];
                snprintf(query, sizeof(query), "KILL QUERY %lu", g->id);
                (void)mysql_query(ctl.my, query);
                mysql_close(ctl.my);
            } else if (ctl.my)
                mysql_close(ctl.my);
            mysql_thread_end();
            return NULL;
        }
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        t.tv_nsec += 10000000;
        if (t.tv_nsec >= 1000000000) {
            t.tv_sec++;
            t.tv_nsec -= 1000000000;
        }
        pthread_cond_timedwait(&g->changed, &g->mutex, &t);
    }
    pthread_mutex_unlock(&g->mutex);
    return NULL;
}
static bool guard_start(Guard *g, DbSession *s, DbError *e) {
    memset(g, 0, sizeof(*g));
    g->s = s;
    g->id = mysql_thread_id(s->my);
    pthread_mutex_init(&g->mutex, NULL);
    pthread_cond_init(&g->changed, NULL);
    if (pthread_create(&g->thread, NULL, watch, g) != 0) {
        pthread_mutex_destroy(&g->mutex);
        pthread_cond_destroy(&g->changed);
        db_error(e, "DB_RESOURCE", "cannot create query cancellation guard");
        return false;
    }
    return true;
}
static bool guard_end(Guard *g, DbError *e) {
    pthread_mutex_lock(&g->mutex);
    atomic_store(&g->done, true);
    pthread_cond_signal(&g->changed);
    pthread_mutex_unlock(&g->mutex);
    pthread_join(g->thread, NULL);
    pthread_mutex_destroy(&g->mutex);
    pthread_cond_destroy(&g->changed);
    if (atomic_load(&g->expired)) {
        g->s->broken = true;
        db_error(e, atomic_load(&g->s->cancelled) ? "DB_CANCELLED" : "DB_TIMEOUT",
                 "query cancelled; connection discarded");
        return false;
    }
    return true;
}
static void ignore_notice(void *context, const char *message) {
    (void)context;
    (void)message;
}
bool db_connect(DbSession *s, DbError *e) {
    DbConfig *c = &s->config;
    if (!strcmp(c->driver, "mysql")) {
        s->my = mysql_init(NULL);
        if (!s->my) {
            db_error(e, "DB_RESOURCE", "cannot allocate MySQL connection");
            return false;
        }
        unsigned protocol = MYSQL_PROTOCOL_TCP;
        mysql_options(s->my, MYSQL_OPT_PROTOCOL, &protocol);
        unsigned seconds = (c->connect_ms + 999) / 1000, io = (c->timeout_ms + 999) / 1000;
        mysql_options(s->my, MYSQL_OPT_CONNECT_TIMEOUT, &seconds);
        mysql_options(s->my, MYSQL_OPT_READ_TIMEOUT, &io);
        mysql_options(s->my, MYSQL_OPT_WRITE_TIMEOUT, &io);
        mysql_options(s->my, MYSQL_SET_CHARSET_NAME, "utf8mb4");
#ifdef MARIADB_BASE_VERSION
        DbBool enforce = strcmp(c->tls, "disabled") != 0,
               verify = !strcmp(c->tls, "verify_ca") || !strcmp(c->tls, "verify_identity");
        if (!strcmp(c->tls, "verify_ca")) {
            db_error(e, "DB_CONFIG",
                     "MariaDB client requires verify_identity rather than verify_ca");
            return false;
        }
        mysql_options(s->my, MYSQL_OPT_SSL_ENFORCE, &enforce);
        mysql_options(s->my, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
#else
        enum mysql_ssl_mode mode = SSL_MODE_VERIFY_IDENTITY;
        if (!strcmp(c->tls, "disabled"))
            mode = SSL_MODE_DISABLED;
        else if (!strcmp(c->tls, "required"))
            mode = SSL_MODE_REQUIRED;
        else if (!strcmp(c->tls, "verify_ca"))
            mode = SSL_MODE_VERIFY_CA;
        mysql_options(s->my, MYSQL_OPT_SSL_MODE, &mode);
#endif
        if (c->ca[0])
            mysql_options(s->my, MYSQL_OPT_SSL_CA, c->ca);
        if (!mysql_real_connect(
                s->my, c->host, c->user, c->password, c->database, (unsigned)atoi(c->port), NULL,
                CLIENT_FOUND_ROWS | CLIENT_MULTI_RESULTS | CLIENT_PS_MULTI_RESULTS)) {
            db_driver_error(s, e, "MySQL connection failed");
            return false;
        }
        if (strcmp(c->tls, "disabled") && mysql_get_ssl_cipher(s->my) == NULL) {
            db_error(e, "DB_TLS", "encrypted connection required");
            return false;
        }
        if (mysql_query(s->my, "SET time_zone = '+00:00'") != 0) {
            db_driver_error(s, e, "cannot initialize database session");
            return false;
        }
    } else {
        char timeout[16];
        snprintf(timeout, sizeof(timeout), "%u", (c->connect_ms + 999) / 1000);
        const char *mode = !strcmp(c->tls, "disabled")    ? "disable"
                           : !strcmp(c->tls, "required")  ? "require"
                           : !strcmp(c->tls, "verify_ca") ? "verify-ca"
                                                          : "verify-full";
        const char *keys[] = {"host",
                              "port",
                              "user",
                              "password",
                              "dbname",
                              "sslmode",
                              "sslrootcert",
                              "connect_timeout",
                              "application_name",
                              NULL};
        const char *vals[] = {c->host,
                              c->port,
                              c->user,
                              c->password,
                              c->database,
                              mode,
                              c->ca[0] ? c->ca : NULL,
                              timeout,
                              "hhy-database",
                              NULL};
        s->pg = PQconnectStartParams(keys, vals, 0);
        if (!s->pg) {
            db_error(e, "DB_RESOURCE", "cannot allocate PostgreSQL connection");
            return false;
        }
        PQsetNoticeProcessor(s->pg, ignore_notice, NULL);
        uint64_t before = s->deadline;
        uint64_t connect_deadline = db_now() + c->connect_ms;
        if (connect_deadline < s->deadline)
            s->deadline = connect_deadline;
        for (;;) {
            PostgresPollingStatusType r = PQconnectPoll(s->pg);
            if (r == PGRES_POLLING_OK)
                break;
            if (r == PGRES_POLLING_FAILED) {
                const char *message = PQerrorMessage(s->pg);
                const char *code =
                    strstr(message, "certificate") || strstr(message, "SSL") ? "DB_TLS"
                    : strstr(message, "password authentication failed")      ? "DB_AUTH"
                                                                             : "DB_CONNECT";
                db_error(e, code, "PostgreSQL connection or authentication failed");
                return false;
            }
            if (!ready(s, PQsocket(s->pg), r == PGRES_POLLING_WRITING ? POLLOUT : POLLIN, e))
                return false;
        }
        s->deadline = before;
        PQsetnonblocking(s->pg, 1);
        if (!db_control(s, "SET TIME ZONE 'UTC'", e))
            return false;
        if (!db_control(s, "SET client_encoding = 'UTF8'", e))
            return false;
    }
    return true;
}
bool db_control(DbSession *s, const char *sql, DbError *e) {
    if (s->my) {
        Guard g;
        if (!guard_start(&g, s, e))
            return false;
        int r = mysql_query(s->my, sql);
        bool ok = guard_end(&g, e);
        if (!ok)
            return false;
        if (r) {
            db_driver_error(s, e, "database control statement failed");
            return false;
        }
        return true;
    }
    if (!PQsendQueryParams(s->pg, sql, 0, NULL, NULL, NULL, NULL, 0)) {
        db_driver_error(s, e, "cannot send database control statement");
        return false;
    }
    PGresult *r = pg_result(s, e);
    bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    if (!r && !e->code[0])
        db_driver_error(s, e, "database control response was lost");
    if (!ok && r) {
        s->pg_pending = r;
        db_driver_error(s, e, "database control statement failed");
        s->pg_pending = NULL;
    }
    if (r)
        PQclear(r);
    PGresult *tail = pg_result(s, e);
    if (tail)
        PQclear(tail);
    return ok && !s->broken;
}
void db_result_close(DbSession *s) {
    if (s->stmt) {
        mysql_stmt_close(s->stmt);
        s->stmt = NULL;
    }
    if (s->pg_pending) {
        PQclear(s->pg_pending);
        s->pg_pending = NULL;
    }
    json_decref(s->columns);
    s->columns = NULL;
    json_decref(s->pending);
    s->pending = NULL;
}
void db_result_finish(DbSession *s) {
    if (!s->prepared_sql) {
        db_result_close(s);
        return;
    }
    if (s->stmt)
        mysql_stmt_free_result(s->stmt);
    if (s->pg_pending) {
        PQclear(s->pg_pending);
        s->pg_pending = NULL;
    }
    json_decref(s->columns);
    s->columns = NULL;
    json_decref(s->pending);
    s->pending = NULL;
}
bool db_reset(DbSession *s) {
    db_result_close(s);
    if (s->broken)
        return false;
    DbError e = {0};
    s->deadline = db_now() + s->config.timeout_ms;
    if (s->my) {
        if (mysql_reset_connection(s->my) != 0)
            return false;
        return mysql_set_character_set(s->my, "utf8mb4") == 0 &&
               db_control(s, "SET time_zone = '+00:00'", &e);
    }
    PGTransactionStatusType t = PQtransactionStatus(s->pg);
    if (t == PQTRANS_ACTIVE || t == PQTRANS_UNKNOWN)
        return false;
    if (t != PQTRANS_IDLE && !db_control(s, "ROLLBACK", &e))
        return false;
    return db_control(s, "DISCARD ALL", &e) && db_control(s, "SET TIME ZONE 'UTC'", &e) &&
           db_control(s, "SET client_encoding = 'UTF8'", &e);
}
static bool hex_decode(const char *src, unsigned char **out, size_t *n) {
    size_t len = strlen(src);
    if (len % 2 || len > DB_MAX_FIELD * 2)
        return false;
    *out = malloc(len / 2 + 1);
    if (!*out)
        return false;
    for (size_t i = 0; i < len; i += 2) {
        unsigned x = 0;
        for (int k = 0; k < 2; k++) {
            char c = src[i + k];
            int v = c >= '0' && c <= '9'   ? c - '0'
                    : c >= 'a' && c <= 'f' ? c - 'a' + 10
                    : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                           : -1;
            if (v < 0) {
                free(*out);
                *out = NULL;
                return false;
            }
            x = x * 16 + (unsigned)v;
        }
        (*out)[i / 2] = (unsigned char)x;
    }
    *n = len / 2;
    return true;
}
static char *hex_encode(const unsigned char *p, size_t n) {
    char *s = malloc(n * 2 + 1);
    if (!s)
        return NULL;
    for (size_t i = 0; i < n; i++)
        snprintf(s + 2 * i, 3, "%02x", p[i]);
    s[n * 2] = 0;
    return s;
}
static bool param_value(json_t *v, unsigned char **out, size_t *n, bool *binary) {
    *out = NULL;
    *n = 0;
    *binary = false;
    if (json_is_null(v))
        return true;
    if (json_is_object(v)) {
        const char *type = json_string_value(json_object_get(v, "type")),
                   *value = json_string_value(json_object_get(v, "value"));
        if (!type || !value)
            return false;
        if (!strcmp(type, "bytes")) {
            *binary = true;
            return hex_decode(value, out, n);
        }
        if (strcmp(type, "decimal") && strcmp(type, "integer") && strcmp(type, "date") &&
            strcmp(type, "time") && strcmp(type, "timestamp") && strcmp(type, "json"))
            return false;
        *n = strlen(value);
        if (*n > DB_MAX_FIELD)
            return false;
        *out = (unsigned char *)strdup(value);
        return *out != NULL;
    }
    if (json_is_string(v)) {
        *n = json_string_length(v);
        if (*n > DB_MAX_FIELD || strlen(json_string_value(v)) != *n)
            return false;
        *out = (unsigned char *)strdup(json_string_value(v));
    } else if (json_is_boolean(v))
        *out = (unsigned char *)strdup(json_is_true(v) ? "1" : "0");
    else if (json_is_number(v))
        *out = (unsigned char *)json_dumps(v, JSON_ENCODE_ANY);
    else
        return false;
    if (!*out)
        return false;
    if (!*n)
        *n = strlen((char *)*out);
    return true;
}
static bool options(DbSession *s, json_t *o, DbError *e) {
    s->typed = false;
    s->positional = false;
    if (o && !json_is_object(o)) {
        db_error(e, "DB_ARGUMENT", "query options must be a map");
        return false;
    }
    if (o) {
        const char *k;
        json_t *v;
        json_object_foreach(o, k, v) {
            if (!strcmp(k, "typed")) {
                if (!json_is_boolean(v))
                    goto invalid;
                s->typed = json_is_true(v);
            } else if (!strcmp(k, "positional")) {
                if (!json_is_boolean(v))
                    goto invalid;
                s->positional = json_is_true(v);
            } else if (!strcmp(k, "timeout_ms")) {
                if (!json_is_integer(v) || json_integer_value(v) < 1 ||
                    json_integer_value(v) > 300000)
                    goto invalid;
                uint64_t d = db_now() + (uint64_t)json_integer_value(v);
                if (d < s->deadline)
                    s->deadline = d;
            } else
                goto invalid;
        }
    }
    return true;
invalid:
    db_error(e, "DB_ARGUMENT", "invalid query option");
    return false;
}
static bool my_columns(DbSession *s, DbError *e) {
    MYSQL_RES *meta = mysql_stmt_result_metadata(s->stmt);
    s->columns = json_array();
    if (!meta) {
        s->result_end = true;
        return mysql_stmt_field_count(s->stmt) == 0;
    }
    unsigned n = mysql_num_fields(meta);
    if (n > DB_MAX_COLUMNS) {
        mysql_free_result(meta);
        db_error(e, "DB_LIMIT", "too many result columns");
        return false;
    }
    MYSQL_FIELD *f = mysql_fetch_fields(meta);
    for (unsigned i = 0; i < n; i++)
        json_array_append_new(s->columns,
                              json_pack("{s:s,s:i,s:i,s:i,s:b}", "name", f[i].name, "type",
                                        f[i].type, "precision", (int)f[i].length, "scale",
                                        (int)f[i].decimals, "binary", f[i].charsetnr == 63));
    mysql_free_result(meta);
    s->result_end = false;
    return true;
}
static bool pg_columns(DbSession *s, PGresult *r, DbError *e) {
    unsigned n = (unsigned)PQnfields(r);
    if (n > DB_MAX_COLUMNS) {
        db_error(e, "DB_LIMIT", "too many result columns");
        return false;
    }
    s->columns = json_array();
    for (unsigned i = 0; i < n; i++)
        json_array_append_new(s->columns,
                              json_pack("{s:s,s:i,s:i}", "name", PQfname(r, (int)i), "type",
                                        (int)PQftype(r, (int)i), "modifier", PQfmod(r, (int)i)));
    return true;
}
bool db_start(DbSession *s, const char *sql, json_t *params, json_t *o, DbError *e) {
    if ((s->stmt && !s->prepared_sql) || s->pg_pending || s->columns) {
        db_error(e, "DB_BUSY", "close the current result before executing another statement");
        return false;
    }
    if (!sql || !sql[0] || strlen(sql) > 65536 || !json_is_array(params) ||
        json_array_size(params) > DB_MAX_PARAMS || !options(s, o, e)) {
        if (!e->code[0])
            db_error(e, "DB_ARGUMENT", "invalid SQL or parameters");
        return false;
    }
    size_t n = json_array_size(params);
    unsigned char **values = calloc(n ? n : 1, sizeof(*values));
    size_t *lengths = calloc(n ? n : 1, sizeof(*lengths));
    bool *binary = calloc(n ? n : 1, sizeof(*binary));
    bool ok = values && lengths && binary;
    for (size_t i = 0; ok && i < n; i++)
        ok = param_value(json_array_get(params, i), &values[i], &lengths[i], &binary[i]);
    if (!ok) {
        db_error(e, "DB_ARGUMENT", "unsupported parameter or field limit exceeded");
        goto end;
    }
    s->affected = 0;
    s->insert_id = 0;
    s->result_end = false;
    s->queries++;
    if (s->my) {
        if (!s->stmt)
            s->stmt = mysql_stmt_init(s->my);
        MYSQL_BIND *b = calloc(n ? n : 1, sizeof(*b));
        unsigned long *lens = calloc(n ? n : 1, sizeof(*lens));
        DbBool *nulls = calloc(n ? n : 1, sizeof(*nulls));
        long long integers[DB_MAX_PARAMS] = {0};
        double reals[DB_MAX_PARAMS] = {0};
        signed char booleans[DB_MAX_PARAMS] = {0};
        if (!s->stmt || !b || !lens || !nulls) {
            free(b);
            free(lens);
            free(nulls);
            db_error(e, "DB_RESOURCE", "cannot allocate statement");
            ok = false;
            goto end;
        }
        for (size_t i = 0; i < n; i++) {
            nulls[i] = values[i] == NULL;
            lens[i] = (unsigned long)lengths[i];
            b[i].is_null = &nulls[i];
            b[i].buffer_type = binary[i] ? MYSQL_TYPE_BLOB : MYSQL_TYPE_STRING;
            b[i].buffer = values[i];
            b[i].buffer_length = lens[i];
            b[i].length = &lens[i];
            json_t *v = json_array_get(params, i);
            if (json_is_integer(v)) {
                integers[i] = (long long)json_integer_value(v);
                b[i].buffer_type = MYSQL_TYPE_LONGLONG;
                b[i].buffer = &integers[i];
                b[i].buffer_length = sizeof(integers[i]);
            } else if (json_is_real(v)) {
                reals[i] = json_real_value(v);
                b[i].buffer_type = MYSQL_TYPE_DOUBLE;
                b[i].buffer = &reals[i];
                b[i].buffer_length = sizeof(reals[i]);
            } else if (json_is_boolean(v)) {
                booleans[i] = json_is_true(v);
                b[i].buffer_type = MYSQL_TYPE_TINY;
                b[i].buffer = &booleans[i];
                b[i].buffer_length = sizeof(booleans[i]);
            } else if (json_is_object(v)) {
                const char *tag = json_string_value(json_object_get(v, "type"));
                if (tag && (!strcmp(tag, "decimal") || !strcmp(tag, "integer")))
                    b[i].buffer_type = MYSQL_TYPE_NEWDECIMAL;
            }
        }
        Guard g;
        ok = guard_start(&g, s, e);
        if (ok) {
            ok = s->prepared_ready ||
                 mysql_stmt_prepare(s->stmt, sql, (unsigned long)strlen(sql)) == 0;
            if (!ok && mysql_stmt_errno(s->stmt) == 1295 && n == 0 && !s->prepared_sql) {
                mysql_stmt_close(s->stmt);
                s->stmt = NULL;
                ok = mysql_real_query(s->my, sql, (unsigned long)strlen(sql)) == 0;
                if (ok && mysql_field_count(s->my) != 0) {
                    db_error(e, "DB_NOT_SUPPORTED",
                             "this statement requires a native result cursor");
                    s->broken = true;
                    ok = false;
                }
                if (!guard_end(&g, e))
                    ok = false;
                free(b);
                free(lens);
                free(nulls);
                if (ok) {
                    s->columns = json_array();
                    s->result_end = true;
                    s->affected = mysql_affected_rows(s->my);
                    s->insert_id = mysql_insert_id(s->my);
                } else if (!e->code[0])
                    db_driver_error(s, e, "MySQL statement failed");
                goto end;
            }
            if (ok && s->prepared_sql)
                s->prepared_ready = true;
            if (ok && mysql_stmt_param_count(s->stmt) != n) {
                db_error(e, "DB_ARGUMENT", "parameter count does not match statement");
                ok = false;
            }
            if (ok)
                ok = (!n || mysql_stmt_bind_param(s->stmt, b) == 0) &&
                     mysql_stmt_execute(s->stmt) == 0;
            if (!guard_end(&g, e))
                ok = false;
        }
        free(b);
        free(lens);
        free(nulls);
        if (!ok) {
            if (!e->code[0])
                db_driver_error(s, e, "MySQL statement failed");
            goto end;
        }
        my_ulonglong affected = mysql_stmt_affected_rows(s->stmt);
        s->affected = affected == (my_ulonglong)-1 ? 0 : affected;
        s->insert_id = mysql_stmt_insert_id(s->stmt);
        ok = my_columns(s, e);
    } else {
        int *formats = calloc(n ? n : 1, sizeof(*formats)),
            *sizes = calloc(n ? n : 1, sizeof(*sizes));
        Oid *types = calloc(n ? n : 1, sizeof(*types));
        if (!formats || !sizes || !types) {
            free(formats);
            free(sizes);
            free(types);
            db_error(e, "DB_RESOURCE", "cannot allocate parameters");
            ok = false;
            goto end;
        }
        for (size_t i = 0; i < n; i++) {
            formats[i] = binary[i] ? 1 : 0;
            sizes[i] = (int)lengths[i];
            types[i] = binary[i] ? 17 : 0;
            json_t *v = json_array_get(params, i);
            if (json_is_integer(v))
                types[i] = 20;
            else if (json_is_real(v))
                types[i] = 701;
            else if (json_is_boolean(v))
                types[i] = 16;
            else if (json_is_object(v)) {
                const char *tag = json_string_value(json_object_get(v, "type"));
                if (tag) {
                    if (!strcmp(tag, "decimal") || !strcmp(tag, "integer"))
                        types[i] = 1700;
                    else if (!strcmp(tag, "json"))
                        types[i] = 114;
                    else if (!strcmp(tag, "date"))
                        types[i] = 1082;
                    else if (!strcmp(tag, "time"))
                        types[i] = 1083;
                    else if (!strcmp(tag, "timestamp"))
                        types[i] = 1184;
                }
            }
        }
        if (s->prepared_sql && !s->prepared_ready) {
            ok = PQsendPrepare(s->pg, "hhy_prepared", sql, (int)n, types) != 0;
            PGresult *prepared = ok ? pg_result(s, e) : NULL;
            ok = prepared && PQresultStatus(prepared) == PGRES_COMMAND_OK;
            if (!ok && prepared) {
                s->pg_pending = prepared;
                db_driver_error(s, e, "prepare failed");
                s->pg_pending = NULL;
            }
            if (prepared)
                PQclear(prepared);
            PGresult *tail = pg_result(s, e);
            if (tail)
                PQclear(tail);
            s->prepared_ready = ok;
        }
        if (ok)
            ok = s->prepared_sql
                     ? PQsendQueryPrepared(s->pg, "hhy_prepared", (int)n,
                                           (const char *const *)values, sizes, formats, 0) != 0
                     : PQsendQueryParams(s->pg, sql, (int)n, types, (const char *const *)values,
                                         sizes, formats, 0) != 0;
        free(formats);
        free(sizes);
        free(types);
        if (ok)
            ok = PQsetSingleRowMode(s->pg) != 0;
        if (!ok) {
            db_driver_error(s, e, "PostgreSQL statement send failed");
            s->broken = true;
            goto end;
        }
        s->pg_pending = pg_result(s, e);
        if (!s->pg_pending) {
            ok = false;
            if (!e->code[0])
                db_driver_error(s, e, "PostgreSQL statement failed");
            goto end;
        }
        ExecStatusType status = PQresultStatus(s->pg_pending);
        if (status != PGRES_SINGLE_TUPLE && status != PGRES_TUPLES_OK &&
            status != PGRES_COMMAND_OK) {
            db_driver_error(s, e, "PostgreSQL statement failed");
            ok = false;
            goto end;
        }
        ok = pg_columns(s, s->pg_pending, e);
        s->result_end = status != PGRES_SINGLE_TUPLE;
        const char *affected = PQcmdTuples(s->pg_pending);
        s->affected = affected[0] ? strtoull(affected, NULL, 10) : 0;
        if (s->result_end) {
            PQclear(s->pg_pending);
            s->pg_pending = NULL;
            PGresult *tail = pg_result(s, e);
            if (tail)
                PQclear(tail);
        }
    }
end:
    if (values)
        for (size_t i = 0; i < n; i++)
            free(values[i]);
    free(values);
    free(lengths);
    free(binary);
    if (!ok) {
        if (s->pg && !s->broken) {
            if (s->pg_pending) {
                PQclear(s->pg_pending);
                s->pg_pending = NULL;
            }
            PGresult *tail;
            while ((tail = pg_result(s, e)) != NULL)
                PQclear(tail);
        }
        db_result_finish(s);
    }
    return ok;
}
static json_t *cell(DbSession *s, const char *value, size_t length, int type, bool binary,
                    DbError *e) {
    if (length > DB_MAX_FIELD) {
        db_error(e, "DB_LIMIT", "database field exceeds 64 KiB");
        return NULL;
    }
    if (binary) {
        unsigned char *decoded = NULL;
        size_t bytes = length;
        if (s->pg) {
            decoded = PQunescapeBytea((const unsigned char *)value, &bytes);
            if (!decoded) {
                db_error(e, "DB_TYPE", "invalid binary value");
                return NULL;
            }
        }
        char *hex = hex_encode(decoded ? decoded : (const unsigned char *)value, bytes);
        if (decoded)
            PQfreemem(decoded);
        json_t *r = hex ? json_pack("{s:s,s:s}", "type", "bytes", "value", hex) : NULL;
        free(hex);
        return r;
    }
    json_t *v = json_stringn(value, length);
    if (!v) {
        db_error(e, "DB_TYPE", "invalid UTF-8 database value");
        return NULL;
    }
    if (!s->typed)
        return v;
    const char *tag = NULL;
    bool integer = false, boolean = false, floating = false;
    if (s->my) {
        integer = type == MYSQL_TYPE_TINY || type == MYSQL_TYPE_SHORT || type == MYSQL_TYPE_LONG ||
                  type == MYSQL_TYPE_LONGLONG || type == MYSQL_TYPE_INT24;
        floating = type == MYSQL_TYPE_FLOAT || type == MYSQL_TYPE_DOUBLE;
        if (type == MYSQL_TYPE_DECIMAL || type == MYSQL_TYPE_NEWDECIMAL)
            tag = "decimal";
        else if (type == MYSQL_TYPE_DATE)
            tag = "date";
        else if (type == MYSQL_TYPE_TIME)
            tag = "time";
        else if (type == MYSQL_TYPE_DATETIME || type == MYSQL_TYPE_TIMESTAMP)
            tag = "timestamp";
        else if (type == MYSQL_TYPE_JSON)
            tag = "json";
    } else {
        integer = type == 20 || type == 21 || type == 23;
        boolean = type == 16;
        floating = type == 700 || type == 701;
        if (type == 1700)
            tag = "decimal";
        else if (type == 1082)
            tag = "date";
        else if (type == 1083 || type == 1266)
            tag = "time";
        else if (type == 1114 || type == 1184)
            tag = "timestamp";
        else if (type == 114 || type == 3802)
            tag = "json";
    }
    if (integer) {
        char *end = NULL;
        errno = 0;
        long long n = strtoll(value, &end, 10);
        if (!errno && end == value + length) {
            json_decref(v);
            return json_integer(n);
        }
        tag = "integer";
    }
    if (boolean) {
        json_decref(v);
        return json_boolean(length == 1 && value[0] == 't');
    }
    if (floating) {
        char *end = NULL;
        double n = strtod(value, &end);
        if (isfinite(n) && end == value + length) {
            json_decref(v);
            return json_real(n);
        }
        tag = "float";
    }
    if (tag) {
        json_t *r = json_pack("{s:s,s:o}", "type", tag, "value", v);
        return r;
    }
    return v;
}
static json_t *row_mysql(DbSession *s, DbError *e) {
    unsigned n = (unsigned)json_array_size(s->columns);
    MYSQL_BIND b[DB_MAX_COLUMNS] = {0};
    unsigned long lengths[DB_MAX_COLUMNS] = {0};
    DbBool nulls[DB_MAX_COLUMNS] = {0}, errors[DB_MAX_COLUMNS] = {0};
    for (unsigned i = 0; i < n; i++) {
        b[i].buffer_type = MYSQL_TYPE_STRING;
        b[i].length = &lengths[i];
        b[i].is_null = &nulls[i];
        b[i].error = &errors[i];
    }
    if (mysql_stmt_bind_result(s->stmt, b)) {
        db_driver_error(s, e, "cannot bind result");
        return NULL;
    }
    int fetched = mysql_stmt_fetch(s->stmt);
    if (fetched == MYSQL_NO_DATA) {
        s->result_end = true;
        return NULL;
    }
    if (fetched != 0 && fetched != MYSQL_DATA_TRUNCATED) {
        db_driver_error(s, e, "cannot fetch result");
        return NULL;
    }
    json_t *row = s->positional ? json_array() : json_object();
    size_t total = 0;
    for (unsigned i = 0; i < n; i++) {
        json_t *col = json_array_get(s->columns, i);
        const char *name = json_string_value(json_object_get(col, "name"));
        if (!s->positional && json_object_get(row, name)) {
            db_error(e, "DB_DUPLICATE_COLUMN",
                     "duplicate result column; use positional mode or SQL aliases");
            goto fail;
        }
        json_t *v = json_null();
        if (!nulls[i]) {
            total += lengths[i];
            if (lengths[i] > DB_MAX_FIELD || total > DB_MAX_RESULT / 4) {
                db_error(e, "DB_LIMIT", "row or field byte budget exceeded");
                goto fail;
            }
            char *buf = calloc(lengths[i] + 1, 1);
            if (!buf) {
                db_error(e, "DB_RESOURCE", "cannot allocate result field");
                goto fail;
            }
            MYSQL_BIND out = {0};
            out.buffer_type = MYSQL_TYPE_STRING;
            out.buffer = buf;
            out.buffer_length = lengths[i];
            if (mysql_stmt_fetch_column(s->stmt, &out, i, 0)) {
                free(buf);
                db_driver_error(s, e, "cannot fetch result field");
                goto fail;
            }
            int type = (int)json_integer_value(json_object_get(col, "type"));
            bool bin = json_is_true(json_object_get(col, "binary")) &&
                       (type == MYSQL_TYPE_BLOB || type == MYSQL_TYPE_TINY_BLOB ||
                        type == MYSQL_TYPE_MEDIUM_BLOB || type == MYSQL_TYPE_LONG_BLOB ||
                        type == MYSQL_TYPE_STRING || type == MYSQL_TYPE_VAR_STRING ||
                        type == MYSQL_TYPE_BIT);
            v = cell(s, buf, lengths[i], type, bin, e);
            free(buf);
            if (!v)
                goto fail;
        }
        if (s->positional)
            json_array_append_new(row, v);
        else
            json_object_set_new(row, name, v);
    }
    return row;
fail:
    json_decref(row);
    return NULL;
}
static json_t *row_postgres(DbSession *s, DbError *e) {
    if (!s->pg_pending)
        s->pg_pending = pg_result(s, e);
    PGresult *r = s->pg_pending;
    if (!r) {
        if (!e->code[0])
            s->result_end = true;
        return NULL;
    }
    ExecStatusType status = PQresultStatus(r);
    if (status == PGRES_TUPLES_OK || status == PGRES_COMMAND_OK) {
        s->result_end = true;
        PQclear(r);
        s->pg_pending = NULL;
        PGresult *tail = pg_result(s, e);
        if (tail)
            PQclear(tail);
        return NULL;
    }
    if (status != PGRES_SINGLE_TUPLE) {
        db_driver_error(s, e, "PostgreSQL fetch failed");
        return NULL;
    }
    json_t *row = s->positional ? json_array() : json_object();
    int n = PQnfields(r);
    size_t total = 0;
    for (int i = 0; i < n; i++) {
        const char *name = PQfname(r, i);
        if (!s->positional && json_object_get(row, name)) {
            db_error(e, "DB_DUPLICATE_COLUMN",
                     "duplicate result column; use positional mode or SQL aliases");
            goto fail;
        }
        total += (size_t)PQgetlength(r, 0, i);
        if (total > DB_MAX_RESULT / 4) {
            db_error(e, "DB_LIMIT", "row byte budget exceeded");
            goto fail;
        }
        json_t *v = PQgetisnull(r, 0, i)
                        ? json_null()
                        : cell(s, PQgetvalue(r, 0, i), (size_t)PQgetlength(r, 0, i),
                               (int)PQftype(r, i), PQftype(r, i) == 17, e);
        if (!v)
            goto fail;
        if (s->positional)
            json_array_append_new(row, v);
        else
            json_object_set_new(row, name, v);
    }
    PQclear(r);
    s->pg_pending = NULL;
    return row;
fail:
    json_decref(row);
    return NULL;
}
static json_t *fetch_rows(DbSession *s, unsigned limit, DbError *e) {
    json_t *rows = json_array();
    size_t bytes = 0;
    while (json_array_size(rows) < limit && !s->result_end) {
        json_t *r = s->pending;
        s->pending = NULL;
        if (!r)
            r = s->my ? row_mysql(s, e) : row_postgres(s, e);
        if (!r) {
            if (e->code[0]) {
                json_decref(rows);
                s->broken = true;
                return NULL;
            }
            break;
        }
        char *encoded = json_dumps(r, JSON_COMPACT | JSON_ENSURE_ASCII);
        if (!encoded) {
            json_decref(r);
            json_decref(rows);
            db_error(e, "DB_RESOURCE", "cannot encode result");
            return NULL;
        }
        size_t n = strlen(encoded);
        free(encoded);
        if (n > DB_MAX_RESULT / 2) {
            json_decref(r);
            json_decref(rows);
            db_error(e, "DB_LIMIT", "encoded row exceeds byte budget");
            s->broken = true;
            return NULL;
        }
        if (bytes + n > DB_MAX_RESULT / 2 && json_array_size(rows)) {
            s->pending = r;
            break;
        }
        bytes += n;
        json_array_append_new(rows, r);
        s->rows++;
    }
    return json_pack("{s:o,s:O,s:b,s:I,s:I}", "rows", rows, "columns",
                     s->columns ? s->columns : json_null(), "done", s->result_end, "row_count",
                     (json_int_t)s->affected, "last_insert_id", (json_int_t)s->insert_id);
}
static json_t *exact_unsigned(uint64_t value) {
    if (value <= INT64_MAX)
        return json_integer((json_int_t)value);
    char text[32];
    snprintf(text, sizeof(text), "%llu", (unsigned long long)value);
    return json_pack("{s:s,s:s}", "type", "integer", "value", text);
}
json_t *db_fetch(DbSession *s, unsigned limit, DbError *e) {
    Guard g;
    bool guarded = s->my && !s->result_end;
    if (guarded && !guard_start(&g, s, e))
        return NULL;
    json_t *r = fetch_rows(s, limit, e);
    if (r) {
        json_object_set_new(r, "last_insert_id", exact_unsigned(s->insert_id));
        json_object_set_new(r, "row_count", exact_unsigned(s->affected));
    }
    if (guarded && !guard_end(&g, e)) {
        json_decref(r);
        return NULL;
    }
    return r;
}
bool db_next_result(DbSession *s, DbError *e) {
    if (!s->result_end) {
        db_error(e, "DB_BUSY", "consume current result before advancing");
        return false;
    }
    if (!s->my) {
        db_error(e, "DB_NOT_SUPPORTED",
                 "PostgreSQL extended queries return one result; use explicit CALL output or "
                 "transaction-scoped refcursors");
        return false;
    }
    json_decref(s->columns);
    s->columns = NULL;
    Guard g;
    if (!guard_start(&g, s, e))
        return false;
    int r = mysql_stmt_next_result(s->stmt);
    if (!guard_end(&g, e))
        return false;
    if (r == -1) {
        s->columns = json_array();
        s->result_end = true;
        return false;
    }
    if (r) {
        db_driver_error(s, e, "cannot advance result set");
        return false;
    }
    my_ulonglong affected = mysql_stmt_affected_rows(s->stmt);
    s->affected = affected == (my_ulonglong)-1 ? 0 : affected;
    s->insert_id = mysql_stmt_insert_id(s->stmt);
    return my_columns(s, e);
}
