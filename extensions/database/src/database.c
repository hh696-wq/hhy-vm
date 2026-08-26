#define _POSIX_C_SOURCE 200809L
#include "database.h"

#include <libpq-fe.h>
#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct { char **owned; const char **values; size_t count; } Parameters;

static void parameters_free(Parameters *parameters) {
    for (size_t i = 0; i < parameters->count; i++) free(parameters->owned[i]);
    free(parameters->owned); free(parameters->values); memset(parameters, 0, sizeof(*parameters));
}

static bool parameters_make(json_t *array, Parameters *parameters, const char **error) {
    if (!json_is_array(array)) { *error = "params must be a list"; return false; }
    parameters->count = json_array_size(array);
    parameters->owned = calloc(parameters->count, sizeof(char *));
    parameters->values = calloc(parameters->count, sizeof(char *));
    if (parameters->count && (parameters->owned == NULL || parameters->values == NULL)) {
        *error = "out of memory"; parameters_free(parameters); return false;
    }
    for (size_t i = 0; i < parameters->count; i++) {
        json_t *value = json_array_get(array, i);
        if (json_is_null(value)) continue;
        if (json_is_string(value)) parameters->owned[i] = strdup(json_string_value(value));
        else if (json_is_true(value)) parameters->owned[i] = strdup("true");
        else if (json_is_false(value)) parameters->owned[i] = strdup("false");
        else if (json_is_number(value)) parameters->owned[i] = json_dumps(value, JSON_ENCODE_ANY);
        else { *error = "params may contain only scalar values"; parameters_free(parameters); return false; }
        if (parameters->owned[i] == NULL) { *error = "out of memory"; parameters_free(parameters); return false; }
        parameters->values[i] = parameters->owned[i];
    }
    return true;
}

static json_t *postgres_run(const char *url, const char *sql, json_t *params,
                            bool query, size_t maximum_rows, const char **error) {
    Parameters parameters = {0};
    if (!parameters_make(params, &parameters, error)) return NULL;
    const char *keywords[] = {"dbname", "connect_timeout", NULL};
    const char *connection_values[] = {url, "30", NULL};
    PGconn *connection = PQconnectdbParams(keywords, connection_values, 1);
    if (PQstatus(connection) != CONNECTION_OK) {
        *error = "PostgreSQL connection failed"; PQfinish(connection); parameters_free(&parameters); return NULL;
    }
    PGresult *timeout = PQexec(connection, "SET statement_timeout = '30000ms'");
    if (PQresultStatus(timeout) != PGRES_COMMAND_OK) {
        *error = "cannot set PostgreSQL statement timeout"; PQclear(timeout);
        PQfinish(connection); parameters_free(&parameters); return NULL;
    }
    PQclear(timeout);
    PGresult *result = PQexecParams(connection, sql, (int)parameters.count, NULL,
                                    parameters.values, NULL, NULL, 0);
    parameters_free(&parameters); ExecStatusType status = PQresultStatus(result);
    if ((query && status != PGRES_TUPLES_OK) || (!query && status != PGRES_COMMAND_OK)) {
        *error = "PostgreSQL statement failed"; PQclear(result); PQfinish(connection); return NULL;
    }
    json_t *response = json_object(); json_object_set_new(response, "ok", json_true());
    if (query) {
        int total = PQntuples(result); size_t count = (size_t)total;
        bool truncated = count > maximum_rows; if (truncated) count = maximum_rows;
        json_t *rows = json_array();
        for (size_t row = 0; row < count; row++) {
            json_t *item = json_object();
            for (int column = 0; column < PQnfields(result); column++)
                json_object_set_new(item, PQfname(result, column), PQgetisnull(result, (int)row, column)
                    ? json_null() : json_string(PQgetvalue(result, (int)row, column)));
            json_array_append_new(rows, item);
        }
        json_object_set_new(response, "rows", rows);
        json_object_set_new(response, "count", json_integer((json_int_t)count));
        json_object_set_new(response, "truncated", json_boolean(truncated));
    } else {
        const char *count = PQcmdTuples(result);
        json_object_set_new(response, "row_count", json_integer(count[0] ? atoll(count) : 0));
    }
    PQclear(result); PQfinish(connection); return response;
}

typedef struct { char host[256], user[256], password[256], database[256]; unsigned int port; } MysqlUrl;

static bool copy_part(char *output, size_t capacity, const char *start, const char *end) {
    size_t length = (size_t)(end - start); if (length >= capacity) return false;
    memcpy(output, start, length); output[length] = '\0'; return true;
}

static bool parse_mysql_url(const char *url, MysqlUrl *parsed) {
    memset(parsed, 0, sizeof(*parsed)); parsed->port = 3306;
    if (strncmp(url, "mysql://", 8) != 0) return false;
    const char *start = url + 8, *at = strchr(start, '@'), *slash = at ? strchr(at, '/') : NULL;
    const char *password = strchr(start, ':');
    if (at == NULL || slash == NULL || password == NULL || password > at) return false;
    if (!copy_part(parsed->user, sizeof(parsed->user), start, password) ||
        !copy_part(parsed->password, sizeof(parsed->password), password + 1, at) ||
        !copy_part(parsed->database, sizeof(parsed->database), slash + 1, url + strlen(url))) return false;
    const char *port = memchr(at + 1, ':', (size_t)(slash - at - 1));
    if (port != NULL) {
        if (!copy_part(parsed->host, sizeof(parsed->host), at + 1, port)) return false;
        char *end = NULL; unsigned long value = strtoul(port + 1, &end, 10);
        if (end != slash || value == 0 || value > 65535) return false; parsed->port = (unsigned int)value;
    } else if (!copy_part(parsed->host, sizeof(parsed->host), at + 1, slash)) return false;
    return parsed->host[0] != '\0' && parsed->database[0] != '\0';
}

static MYSQL *mysql_connect_url(const char *url, const char **error) {
    MysqlUrl parsed;
    if (!parse_mysql_url(url, &parsed)) { *error = "invalid MySQL URL"; return NULL; }
    MYSQL *connection = mysql_init(NULL); unsigned int timeout = 30;
    if (connection == NULL) { *error = "cannot initialize MySQL client"; return NULL; }
    mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(connection, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(connection, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    if (mysql_real_connect(connection, parsed.host, parsed.user, parsed.password, parsed.database,
                           parsed.port, NULL, CLIENT_FOUND_ROWS) == NULL) {
        *error = "MySQL connection failed"; mysql_close(connection); return NULL;
    }
    return connection;
}

static bool transaction_sql_allowed(const char *sql) {
    while (*sql == ' ' || *sql == '\t' || *sql == '\r' || *sql == '\n') sql++;
    static const char *verbs[] = {"INSERT", "UPDATE", "DELETE"};
    for (size_t i = 0; i < sizeof(verbs) / sizeof(verbs[0]); i++) {
        size_t length = strlen(verbs[i]);
        if (strncasecmp(sql, verbs[i], length) == 0 &&
            (sql[length] == ' ' || sql[length] == '\t' || sql[length] == '\r' ||
             sql[length] == '\n' || sql[length] == '\0')) return true;
    }
    return false;
}

static bool transaction_item(json_t *item, const char **sql, json_t **params,
                             const char **error) {
    if (!json_is_object(item)) { *error = "transaction statement must be a Map"; return false; }
    *sql = json_string_value(json_object_get(item, "sql"));
    *params = json_object_get(item, "params");
    if (*sql == NULL || (*sql)[0] == '\0' || !json_is_array(*params)) {
        *error = "transaction statement requires sql string and params list"; return false;
    }
    if (!transaction_sql_allowed(*sql)) {
        *error = "transaction supports INSERT, UPDATE, and DELETE only"; return false;
    }
    return true;
}

static bool mysql_transaction_execute(MYSQL *connection, const char *sql, json_t *params,
                                      json_int_t *row_count, const char **error) {
    size_t count = json_array_size(params); MYSQL_STMT *statement = mysql_stmt_init(connection);
    if (statement == NULL || mysql_stmt_prepare(statement, sql, strlen(sql)) != 0 ||
        mysql_stmt_param_count(statement) != count) {
        *error = "MySQL transaction statement preparation failed";
        if (statement) mysql_stmt_close(statement);
        return false;
    }
    MYSQL_BIND *inputs = calloc(count, sizeof(*inputs));
    unsigned long *lengths = calloc(count, sizeof(*lengths)); bool *nulls = calloc(count, sizeof(*nulls));
    signed char *booleans = calloc(count, sizeof(*booleans));
    long long *integers = calloc(count, sizeof(*integers)); double *numbers = calloc(count, sizeof(*numbers));
    if (count && (inputs == NULL || lengths == NULL || nulls == NULL || booleans == NULL ||
                  integers == NULL || numbers == NULL)) {
        *error = "out of memory"; free(inputs); free(lengths); free(nulls); free(booleans);
        free(integers); free(numbers); mysql_stmt_close(statement); return false;
    }
    bool valid = true;
    for (size_t i = 0; i < count; i++) {
        json_t *value = json_array_get(params, i); inputs[i].is_null = &nulls[i];
        if (json_is_null(value)) { nulls[i] = true; inputs[i].buffer_type = MYSQL_TYPE_NULL; }
        else if (json_is_boolean(value)) {
            booleans[i] = json_is_true(value); inputs[i].buffer_type = MYSQL_TYPE_TINY;
            inputs[i].buffer = &booleans[i]; inputs[i].buffer_length = sizeof(booleans[i]);
        } else if (json_is_integer(value)) {
            integers[i] = (long long)json_integer_value(value); inputs[i].buffer_type = MYSQL_TYPE_LONGLONG;
            inputs[i].buffer = &integers[i]; inputs[i].buffer_length = sizeof(integers[i]);
        } else if (json_is_real(value)) {
            numbers[i] = json_real_value(value); inputs[i].buffer_type = MYSQL_TYPE_DOUBLE;
            inputs[i].buffer = &numbers[i]; inputs[i].buffer_length = sizeof(numbers[i]);
        } else if (json_is_string(value)) {
            lengths[i] = (unsigned long)json_string_length(value); inputs[i].buffer_type = MYSQL_TYPE_STRING;
            inputs[i].buffer = (void *)json_string_value(value); inputs[i].buffer_length = lengths[i];
            inputs[i].length = &lengths[i];
        } else { valid = false; break; }
    }
    bool ok = valid && (!count || mysql_stmt_bind_param(statement, inputs) == 0) &&
              mysql_stmt_execute(statement) == 0 && mysql_stmt_field_count(statement) == 0;
    if (!valid) *error = "params may contain only scalar values";
    else if (!ok) *error = "MySQL transaction statement failed";
    if (ok) {
        my_ulonglong affected = mysql_stmt_affected_rows(statement);
        *row_count = affected == (my_ulonglong)-1 ? 0 : (json_int_t)affected;
    }
    free(inputs); free(lengths); free(nulls); free(booleans); free(integers); free(numbers);
    mysql_stmt_close(statement); return ok;
}

static json_t *mysql_run(const char *url, const char *sql, json_t *params,
                         bool query, size_t maximum_rows, const char **error) {
    if (!json_is_array(params)) { *error = "params must be a list"; return NULL; }
    size_t parameter_count = json_array_size(params);
    MYSQL *connection = mysql_connect_url(url, error);
    if (connection == NULL) return NULL;
    MYSQL_STMT *statement = mysql_stmt_init(connection);
    if (statement == NULL || mysql_stmt_prepare(statement, sql, strlen(sql)) != 0 ||
        mysql_stmt_param_count(statement) != parameter_count) {
        *error = "MySQL statement preparation failed";
        if (statement) mysql_stmt_close(statement);
        mysql_close(connection);
        return NULL;
    }
    bool update_max_length = true;
    if (query && mysql_stmt_attr_set(statement, STMT_ATTR_UPDATE_MAX_LENGTH,
                                     &update_max_length) != 0) {
        *error = "cannot configure MySQL result metadata";
        mysql_stmt_close(statement); mysql_close(connection); return NULL;
    }
    MYSQL_BIND *inputs = calloc(parameter_count, sizeof(*inputs));
    unsigned long *lengths = calloc(parameter_count, sizeof(*lengths));
    bool *nulls = calloc(parameter_count, sizeof(*nulls));
    signed char *booleans = calloc(parameter_count, sizeof(*booleans));
    long long *integers = calloc(parameter_count, sizeof(*integers));
    double *numbers = calloc(parameter_count, sizeof(*numbers));
    if (parameter_count && (inputs == NULL || lengths == NULL || nulls == NULL ||
                            booleans == NULL || integers == NULL || numbers == NULL)) {
        *error = "out of memory"; free(inputs); free(lengths); free(nulls);
        free(booleans); free(integers); free(numbers);
        mysql_stmt_close(statement); mysql_close(connection); return NULL;
    }
    bool invalid = false;
    for (size_t i = 0; i < parameter_count; i++) {
        json_t *value = json_array_get(params, i); inputs[i].is_null = &nulls[i];
        if (json_is_null(value)) { nulls[i] = true; inputs[i].buffer_type = MYSQL_TYPE_NULL; }
        else if (json_is_boolean(value)) {
            booleans[i] = json_is_true(value); inputs[i].buffer_type = MYSQL_TYPE_TINY;
            inputs[i].buffer = &booleans[i]; inputs[i].buffer_length = sizeof(booleans[i]);
        } else if (json_is_integer(value)) {
            integers[i] = (long long)json_integer_value(value); inputs[i].buffer_type = MYSQL_TYPE_LONGLONG;
            inputs[i].buffer = &integers[i]; inputs[i].buffer_length = sizeof(integers[i]);
        } else if (json_is_real(value)) {
            numbers[i] = json_real_value(value); inputs[i].buffer_type = MYSQL_TYPE_DOUBLE;
            inputs[i].buffer = &numbers[i]; inputs[i].buffer_length = sizeof(numbers[i]);
        } else if (json_is_string(value)) {
            lengths[i] = (unsigned long)json_string_length(value); inputs[i].buffer_type = MYSQL_TYPE_STRING;
            inputs[i].buffer = (void *)json_string_value(value); inputs[i].buffer_length = lengths[i];
            inputs[i].length = &lengths[i];
        } else { invalid = true; break; }
    }
    bool failed = invalid || (parameter_count && mysql_stmt_bind_param(statement, inputs) != 0) ||
                  mysql_stmt_execute(statement) != 0;
    free(inputs); free(lengths); free(nulls); free(booleans); free(integers); free(numbers);
    if (failed) { *error = invalid ? "params may contain only scalar values" : "MySQL statement failed";
        mysql_stmt_close(statement); mysql_close(connection); return NULL; }
    json_t *response = json_object(); json_object_set_new(response, "ok", json_true());
    if (!query) {
        json_object_set_new(response, "row_count",
                            json_integer((json_int_t)mysql_stmt_affected_rows(statement)));
    } else {
        MYSQL_RES *metadata = mysql_stmt_result_metadata(statement);
        if (metadata == NULL || mysql_stmt_store_result(statement) != 0) {
            *error = "MySQL query did not return rows"; if (metadata) mysql_free_result(metadata);
            json_decref(response); mysql_stmt_close(statement); mysql_close(connection); return NULL;
        }
        unsigned int columns = mysql_num_fields(metadata); MYSQL_FIELD *fields = mysql_fetch_fields(metadata);
        MYSQL_BIND *outputs = calloc(columns, sizeof(*outputs)); char **buffers = calloc(columns, sizeof(*buffers));
        unsigned long *capacities = calloc(columns, sizeof(*capacities));
        unsigned long *actual = calloc(columns, sizeof(*actual)); bool *is_null = calloc(columns, sizeof(*is_null));
        if (outputs == NULL || buffers == NULL || capacities == NULL || actual == NULL || is_null == NULL) {
            *error = "out of memory"; free(outputs); free(buffers); free(capacities); free(actual); free(is_null);
            mysql_free_result(metadata); json_decref(response); mysql_stmt_close(statement); mysql_close(connection); return NULL;
        }
        for (unsigned int i = 0; i < columns; i++) {
            capacities[i] = fields[i].max_length + 1; buffers[i] = calloc(capacities[i], 1);
            outputs[i].buffer_type = MYSQL_TYPE_STRING; outputs[i].buffer = buffers[i];
            outputs[i].buffer_length = capacities[i]; outputs[i].length = &actual[i]; outputs[i].is_null = &is_null[i];
        }
        if (mysql_stmt_bind_result(statement, outputs) != 0) { *error = "cannot bind MySQL results"; failed = true; }
        json_t *rows = json_array(); size_t count = 0; int fetch;
        while (!failed && count < maximum_rows && (fetch = mysql_stmt_fetch(statement)) != MYSQL_NO_DATA) {
            if (fetch == 1 || fetch == MYSQL_DATA_TRUNCATED) { *error = "MySQL result exceeds buffer limits"; failed = true; break; }
            json_t *item = json_object();
            for (unsigned int i = 0; i < columns; i++) json_object_set_new(item, fields[i].name,
                is_null[i] ? json_null() : json_stringn(buffers[i], actual[i]));
            json_array_append_new(rows, item); count++;
        }
        bool truncated = mysql_stmt_num_rows(statement) > count;
        for (unsigned int i = 0; i < columns; i++) free(buffers[i]);
        free(outputs); free(buffers); free(capacities); free(actual); free(is_null); mysql_free_result(metadata);
        if (failed) { json_decref(rows); json_decref(response); response = NULL; }
        else {
            json_object_set_new(response, "rows", rows);
            json_object_set_new(response, "count", json_integer((json_int_t)count));
            json_object_set_new(response, "truncated", json_boolean(truncated));
        }
    }
    mysql_stmt_close(statement); mysql_close(connection); return response;
}

json_t *database_ping(const char *url, const char **error) {
    json_t *params = json_array();
    json_t *result = database_query(url, "SELECT 1 AS value", params, 1, error);
    json_decref(params); return result;
}

static bool local_database_url(const char *url) {
    const char *scheme = strstr(url, "://");
    if (scheme == NULL) return false;
    const char *authority = scheme + 3, *slash = strchr(authority, '/');
    if (slash == NULL) return false;
    const char *at = memchr(authority, '@', (size_t)(slash - authority));
    const char *host = at == NULL ? authority : at + 1;
    if (host < slash && *host == '[') return (size_t)(slash - host) >= 5 && strncmp(host, "[::1]", 5) == 0;
    const char *end = memchr(host, ':', (size_t)(slash - host));
    if (end == NULL) end = slash;
    size_t length = (size_t)(end - host);
    return (length == 9 && memcmp(host, "localhost", 9) == 0) ||
           (length == 9 && memcmp(host, "127.0.0.1", 9) == 0);
}

json_t *database_query(const char *url, const char *sql, json_t *params,
                       size_t maximum_rows, const char **error) {
    if (!local_database_url(url)) {
        *error = "database host is outside the installed network capability"; return NULL;
    }
    if (strncmp(url, "postgresql://", 13) == 0 || strncmp(url, "postgres://", 11) == 0)
        return postgres_run(url, sql, params, true, maximum_rows, error);
    if (strncmp(url, "mysql://", 8) == 0) return mysql_run(url, sql, params, true, maximum_rows, error);
    *error = "database URL must use postgresql://, postgres://, or mysql://"; return NULL;
}

json_t *database_execute(const char *url, const char *sql, json_t *params,
                         const char **error) {
    if (!local_database_url(url)) {
        *error = "database host is outside the installed network capability"; return NULL;
    }
    if (strncmp(url, "postgresql://", 13) == 0 || strncmp(url, "postgres://", 11) == 0)
        return postgres_run(url, sql, params, false, 0, error);
    if (strncmp(url, "mysql://", 8) == 0) return mysql_run(url, sql, params, false, 0, error);
    *error = "database URL must use postgresql://, postgres://, or mysql://"; return NULL;
}

static json_t *postgres_transaction(const char *url, json_t *statements,
                                    const char **error) {
    const char *keywords[] = {"dbname", "connect_timeout", NULL};
    const char *values[] = {url, "30", NULL}; PGconn *connection = PQconnectdbParams(keywords, values, 1);
    if (PQstatus(connection) != CONNECTION_OK) {
        *error = "PostgreSQL connection failed"; PQfinish(connection); return NULL;
    }
    PGresult *control = PQexec(connection, "BEGIN");
    bool ok = PQresultStatus(control) == PGRES_COMMAND_OK; PQclear(control);
    if (!ok) *error = "cannot start PostgreSQL transaction";
    json_t *results = json_array();
    for (size_t i = 0; ok && i < json_array_size(statements); i++) {
        const char *sql = NULL; json_t *params = NULL;
        if (!transaction_item(json_array_get(statements, i), &sql, &params, error)) { ok = false; break; }
        Parameters parameters = {0};
        if (!parameters_make(params, &parameters, error)) { ok = false; break; }
        PGresult *result = PQexecParams(connection, sql, (int)parameters.count, NULL,
                                        parameters.values, NULL, NULL, 0);
        parameters_free(&parameters);
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            *error = "PostgreSQL transaction statement failed"; ok = false; PQclear(result); break;
        }
        const char *affected = PQcmdTuples(result);
        json_array_append_new(results, json_pack("{s:I}", "row_count",
            (json_int_t)(affected[0] ? atoll(affected) : 0)));
        PQclear(result);
    }
    if (ok) {
        control = PQexec(connection, "COMMIT"); ok = PQresultStatus(control) == PGRES_COMMAND_OK;
        if (!ok) *error = "PostgreSQL transaction commit failed";
        PQclear(control);
    }
    if (!ok) { control = PQexec(connection, "ROLLBACK"); PQclear(control); }
    PQfinish(connection);
    if (!ok) { json_decref(results); return NULL; }
    return json_pack("{s:b,s:o}", "ok", 1, "results", results);
}

static json_t *mysql_transaction(const char *url, json_t *statements,
                                 const char **error) {
    MYSQL *connection = mysql_connect_url(url, error);
    if (connection == NULL) return NULL;
    bool ok = mysql_autocommit(connection, false) == 0; json_t *results = json_array();
    if (!ok) *error = "cannot start MySQL transaction";
    for (size_t i = 0; ok && i < json_array_size(statements); i++) {
        const char *sql = NULL; json_t *params = NULL; json_int_t row_count = 0;
        if (!transaction_item(json_array_get(statements, i), &sql, &params, error) ||
            !mysql_transaction_execute(connection, sql, params, &row_count, error)) {
            ok = false; break;
        }
        json_array_append_new(results, json_pack("{s:I}", "row_count", row_count));
    }
    if (ok && mysql_commit(connection) != 0) { *error = "MySQL transaction commit failed"; ok = false; }
    if (!ok) (void)mysql_rollback(connection);
    (void)mysql_autocommit(connection, true); mysql_close(connection);
    if (!ok) { json_decref(results); return NULL; }
    return json_pack("{s:b,s:o}", "ok", 1, "results", results);
}

json_t *database_transaction(const char *url, json_t *statements,
                             const char **error) {
    if (!local_database_url(url)) {
        *error = "database host is outside the installed network capability"; return NULL;
    }
    size_t count = json_is_array(statements) ? json_array_size(statements) : 0;
    if (count == 0 || count > 100) {
        *error = "transaction requires between 1 and 100 statements"; return NULL;
    }
    if (strncmp(url, "postgresql://", 13) == 0 || strncmp(url, "postgres://", 11) == 0)
        return postgres_transaction(url, statements, error);
    if (strncmp(url, "mysql://", 8) == 0) return mysql_transaction(url, statements, error);
    *error = "database URL must use postgresql://, postgres://, or mysql://"; return NULL;
}
