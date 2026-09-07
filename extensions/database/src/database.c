#define _POSIX_C_SOURCE 200809L
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static const char *str(json_t *v) {
    return json_is_string(v) && strlen(json_string_value(v)) == json_string_length(v)
               ? json_string_value(v)
               : NULL;
}
static bool count(json_t *args, size_t min, size_t max, DbError *e) {
    if (!json_is_array(args) || json_array_size(args) < min || json_array_size(args) > max) {
        db_error(e, "DB_ARGUMENT", "invalid database argument count");
        return false;
    }
    return true;
}
static bool tx_begin(DbSession *s, json_t *opts, DbError *e) {
    if (s->transaction || s->columns) {
        db_error(e, "DB_BUSY", "resource already has an active transaction or result");
        return false;
    }
    const char *isolation = "READ COMMITTED";
    bool ro = false;
    if (opts) {
        if (!json_is_object(opts)) {
            db_error(e, "DB_ARGUMENT", "transaction options must be a map");
            return false;
        }
        const char *k;
        json_t *v;
        json_object_foreach(opts, k, v) {
            if (!strcmp(k, "isolation")) {
                isolation = str(v);
                if (!isolation ||
                    (strcmp(isolation, "READ COMMITTED") && strcmp(isolation, "REPEATABLE READ") &&
                     strcmp(isolation, "SERIALIZABLE") && strcmp(isolation, "READ UNCOMMITTED")))
                    goto invalid;
            } else if (!strcmp(k, "read_only")) {
                if (!json_is_boolean(v))
                    goto invalid;
                ro = json_is_true(v);
            } else
                goto invalid;
        }
    }
    char sql[128];
    if (s->my) {
        snprintf(sql, sizeof(sql), "SET TRANSACTION ISOLATION LEVEL %s", isolation);
        if (!db_control(s, sql, e))
            return false;
        snprintf(sql, sizeof(sql), "START TRANSACTION %s", ro ? "READ ONLY" : "READ WRITE");
    } else
        snprintf(sql, sizeof(sql), "BEGIN ISOLATION LEVEL %s %s", isolation,
                 ro ? "READ ONLY" : "READ WRITE");
    if (!db_control(s, sql, e))
        return false;
    s->transaction = true;
    if (s->handle[0] && s->resource_deadline > s->deadline)
        s->resource_deadline = s->deadline;
    return true;
invalid:
    db_error(e, "DB_ARGUMENT", "invalid transaction option");
    return false;
}
static bool tx_end(DbSession *s, bool commit, DbError *e) {
    if (!s->transaction) {
        db_error(e, "DB_RESOURCE", "resource has no active transaction");
        return false;
    }
    if (s->columns) {
        db_error(e, "DB_BUSY", "close or consume the result before ending transaction");
        return false;
    }
    bool ok = db_control(s, commit ? "COMMIT" : "ROLLBACK", e);
    if (!ok && commit &&
        (s->broken || !strncmp(e->state, "08", 2) || !strcmp(e->code, "DB_TIMEOUT")))
        db_error(e, "DB_COMMIT_UNKNOWN",
                 "commit outcome is unknown; do not automatically repeat writes");
    s->transaction = false;
    return ok;
}
static json_t *run(DbSession *s, const char *sql, json_t *params, json_t *opts, bool query,
                   unsigned limit, DbError *e) {
    if (s->transaction) {
        const char *p = sql;
        if (!p) {
            db_error(e, "DB_ARGUMENT", "SQL string required");
            return NULL;
        }
        while (isspace((unsigned char)*p))
            p++;
        const char *allowed[] = {"SELECT", "INSERT", "UPDATE", "DELETE", "WITH", NULL};
        bool safe = false;
        for (int i = 0; allowed[i]; i++) {
            size_t n = strlen(allowed[i]);
            if (!strncasecmp(p, allowed[i], n) && !isalnum((unsigned char)p[n]) && p[n] != '_')
                safe = true;
        }
        if (s->pg && (!strncasecmp(p, "CALL ", 5) || !strncasecmp(p, "FETCH ", 6) ||
                      !strncasecmp(p, "MOVE ", 5) || !strncasecmp(p, "CLOSE ", 6)))
            safe = true;
        if (!safe) {
            db_error(e, "DB_TRANSACTION_SQL",
                     "transaction SQL must start with SELECT INSERT UPDATE DELETE or WITH; use "
                     "dedicated transaction controls");
            return NULL;
        }
    }
    if (!db_start(s, sql, params, opts, e))
        return NULL;
    if (!query && json_array_size(s->columns)) {
        db_error(e, "DB_RESULT_EXPECTED", "statement returns rows; use query or cursor");
        s->broken = true;
        return NULL;
    }
    json_t *r = db_fetch(s, limit, e);
    if (!r)
        return NULL;
    bool done = s->result_end;
    if (query) {
        json_object_set_new(r, "count",
                            json_integer((json_int_t)json_array_size(json_object_get(r, "rows"))));
        json_object_set_new(r, "truncated", json_boolean(!done));
    }
    json_object_set_new(r, "ok", json_true());
    /* An unfinished response must never be returned to the idle pool. */
    if (!done)
        s->broken = true;
    db_result_finish(s);
    return r;
}
json_t *db_dispatch(const char *name, json_t *args, const char *scope, const char *request,
                    DbError *e) {
    if (!strcmp(name, "database.stats")) {
        if (!count(args, 0, 0, e))
            return NULL;
        return db_stats();
    }
    if (!strcmp(name, "database.capabilities")) {
        if (!count(args, 0, 0, e))
            return NULL;
        return json_pack("{s:s,s:b,s:b,s:b,s:b,s:b,s:s}", "version", DB_VERSION, "pool", 1,
                         "transactions", 1, "cursor", 1, "mysql_multiple_results", 1, "typed", 1,
                         "protocol", "1.0+scoped-resources");
    }
    if (!count(args, 1, 4, e))
        return NULL;
    json_t *src = json_array_get(args, 0), *a1 = json_array_get(args, 1),
           *a2 = json_array_get(args, 2), *a3 = json_array_get(args, 3), *out = NULL;
    bool known = false;
    const char *names[] = {"connection",
                           "ping",
                           "query",
                           "execute",
                           "transaction",
                           "begin",
                           "commit",
                           "rollback",
                           "savepoint",
                           "rollback_to",
                           "release_savepoint",
                           "cursor",
                           "fetch",
                           "next_result",
                           "close",
                           "close_result",
                           "prepare",
                           "execute_prepared",
                           "query_prepared",
                           "execute_many",
                           NULL};
    const char *op = !strncmp(name, "database.", 9) ? name + 9 : "";
    for (int i = 0; names[i]; i++)
        if (!strcmp(op, names[i]))
            known = true;
    if (!known) {
        db_error(e, "DB_ARGUMENT", "unknown database operation");
        return NULL;
    }
    DbSession *s = db_acquire(src, scope, request, e);
    if (!s)
        return NULL;
    if (!strcmp(op, "connection")) {
        if (!count(args, 1, 1, e))
            goto end;
        s->dedicated = true;
        db_pin(s);
        out = json_string(s->handle);
    } else if (!strcmp(op, "prepare")) {
        if (!count(args, 2, 2, e))
            goto end;
        const char *sql = str(a1);
        if (!sql || !sql[0] || strlen(sql) > 65536 || s->prepared_sql || s->columns ||
            s->transaction) {
            db_error(e, "DB_ARGUMENT", "prepare requires an idle data source and a SQL string");
            goto end;
        }
        s->prepared_sql = strdup(sql);
        if (!s->prepared_sql) {
            db_error(e, "DB_RESOURCE", "cannot allocate prepared statement");
            goto end;
        }
        db_pin(s);
        out = json_string(s->handle);
    } else if (!strcmp(op, "query_prepared") || !strcmp(op, "execute_prepared")) {
        if (!count(args, 2, 3, e))
            goto end;
        if (!s->prepared_sql) {
            db_error(e, "DB_RESOURCE", "resource is not a prepared statement");
            goto end;
        }
        out = run(s, s->prepared_sql, a1, a2, !strcmp(op, "query_prepared"), 10000, e);
    } else if (!strcmp(op, "ping")) {
        if (!count(args, 1, 1, e))
            goto end;
        json_t *p = json_array();
        out = run(s, "SELECT 1 AS value", p, NULL, true, 2, e);
        json_decref(p);
    } else if (!strcmp(op, "query") || !strcmp(op, "execute")) {
        bool query = !strcmp(op, "query");
        if (!count(args, 3, query ? 4 : 3, e))
            goto end;
        unsigned limit = 10000;
        json_t *opts = NULL;
        if (a3) {
            if (json_is_integer(a3) && json_integer_value(a3) > 0)
                limit = json_integer_value(a3) > 10000 ? 10000 : (unsigned)json_integer_value(a3);
            else if (json_is_object(a3))
                opts = a3;
            else {
                db_error(e, "DB_ARGUMENT",
                         "maximum rows must be positive or query options must be a map");
                goto end;
            }
        }
        out = run(s, str(a1), a2, opts, query, limit, e);
    } else if (!strcmp(op, "begin")) {
        if (!count(args, 1, 2, e) || !tx_begin(s, a1, e))
            goto end;
        db_pin(s);
        out = json_string(s->handle);
    } else if (!strcmp(op, "commit") || !strcmp(op, "rollback")) {
        if (!count(args, 1, 1, e) || !tx_end(s, !strcmp(op, "commit"), e))
            goto end;
        if (!s->dedicated)
            s->handle[0] = 0;
        out = json_true();
    } else if (!strcmp(op, "savepoint") || !strcmp(op, "rollback_to") ||
               !strcmp(op, "release_savepoint")) {
        if (!count(args, 2, 2, e))
            goto end;
        const char *label = str(a1);
        if (!s->transaction || !label || !label[0] || strlen(label) > 48) {
            db_error(e, "DB_ARGUMENT", "savepoint requires transaction and a short identifier");
            goto end;
        }
        for (const char *p = label; *p; p++)
            if (!isalnum((unsigned char)*p) && *p != '_') {
                db_error(e, "DB_ARGUMENT", "invalid savepoint identifier");
                goto end;
            }
        char sql[96];
        snprintf(sql, sizeof(sql), "%s %s",
                 !strcmp(op, "savepoint")     ? "SAVEPOINT"
                 : !strcmp(op, "rollback_to") ? "ROLLBACK TO SAVEPOINT"
                                              : "RELEASE SAVEPOINT",
                 label);
        if (db_control(s, sql, e))
            out = json_true();
    } else if (!strcmp(op, "cursor")) {
        if (!count(args, 3, 4, e) || !db_start(s, str(a1), a2, a3, e))
            goto end;
        db_pin(s);
        out = json_string(s->handle);
    } else if (!strcmp(op, "fetch")) {
        if (!count(args, 1, 2, e))
            goto end;
        unsigned limit = 100;
        if (a1) {
            if (!json_is_integer(a1) || json_integer_value(a1) < 1 ||
                json_integer_value(a1) > 10000) {
                db_error(e, "DB_ARGUMENT", "fetch size must be between 1 and 10000");
                goto end;
            }
            limit = (unsigned)json_integer_value(a1);
        }
        if (!s->columns) {
            db_error(e, "DB_RESOURCE", "resource has no result");
            goto end;
        }
        out = db_fetch(s, limit, e);
    } else if (!strcmp(op, "next_result")) {
        if (!count(args, 1, 1, e))
            goto end;
        if (!s->columns) {
            db_error(e, "DB_RESOURCE", "resource has no result");
            goto end;
        }
        bool next = db_next_result(s, e);
        if (!e->code[0])
            out = json_boolean(next);
    } else if (!strcmp(op, "close_result")) {
        if (!count(args, 1, 1, e))
            goto end;
        if (s->columns && !s->result_end)
            s->broken = true;
        db_result_finish(s);
        if (!s->transaction && !s->prepared_sql && !s->dedicated)
            s->handle[0] = 0;
        out = json_true();
    } else if (!strcmp(op, "close")) {
        if (!count(args, 1, 1, e))
            goto end;
        if (s->columns && !s->result_end)
            s->broken = true;
        db_result_close(s);
        if (s->transaction && !s->broken) {
            if (!tx_end(s, false, e))
                s->broken = true;
        }
        free(s->prepared_sql);
        s->prepared_sql = NULL;
        s->prepared_ready = false;
        s->handle[0] = 0;
        out = json_true();
    } else if (!strcmp(op, "transaction") || !strcmp(op, "execute_many")) {
        bool batch = !strcmp(op, "execute_many");
        if (!count(args, batch ? 3 : 2, batch ? 3 : 2, e))
            goto end;
        json_t *items = batch ? a2 : a1;
        size_t n = json_array_size(items);
        if (!json_is_array(items) || n < 1 || n > 100) {
            db_error(e, "DB_ARGUMENT", "batch requires 1 to 100 statements");
            goto end;
        }
        if (!tx_begin(s, NULL, e))
            goto end;
        json_t *results = json_array();
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_array_get(items, i);
            const char *sql = batch ? str(a1) : str(json_object_get(item, "sql"));
            json_t *params = batch ? item : json_object_get(item, "params");
            if (!sql) {
                db_error(e, "DB_ARGUMENT", "batch statement requires SQL");
                break;
            }
            const char *p = sql;
            while (isspace((unsigned char)*p))
                p++;
            if (!batch && strncasecmp(p, "INSERT", 6) && strncasecmp(p, "UPDATE", 6) &&
                strncasecmp(p, "DELETE", 6)) {
                db_error(e, "DB_ARGUMENT",
                         "legacy batch transaction accepts INSERT UPDATE DELETE only");
                break;
            }
            json_t *r = run(s, sql, params, NULL, false, 1, e);
            if (!r)
                break;
            json_array_append_new(results, r);
        }
        if (e->code[0]) {
            DbError ignored = {0};
            if (!s->broken)
                (void)tx_end(s, false, &ignored);
            json_decref(results);
        } else if (tx_end(s, true, e))
            out = json_pack("{s:b,s:o}", "ok", 1, "results", results);
        else
            json_decref(results);
    }
end:
    if (e->code[0] && !out &&
        s->transaction) { /* The explicit owner may recover a server error using a savepoint. */
    }
    db_release(s);
    return out;
}
