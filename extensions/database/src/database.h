#ifndef HHY_DATABASE_H
#define HHY_DATABASE_H
#include <jansson.h>
#include <libpq-fe.h>
#include <mysql.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#define DB_VERSION "1.0.0-rc.1"
#define DB_MAX_SESSIONS 64
#define DB_MAX_FIELD 65536
#define DB_MAX_RESULT 524288
#define DB_MAX_PARAMS 1024
#define DB_MAX_COLUMNS 256

typedef struct {
    char code[64], state[6];
    unsigned int vendor;
    const char *message;
} DbError;
typedef struct {
    char driver[16], host[256], user[256], password[1024], database[256], port[8];
    char tls[24], ca[1024];
    unsigned max_open, max_idle, idle_ms, lifetime_ms, connect_ms, acquire_ms, timeout_ms;
} DbConfig;
typedef struct {
    bool used, busy, transaction, broken, typed, positional, result_end;
    DbConfig config;
    char key[65], handle[65], scope[96], request[96];
    uint64_t born, touched, deadline, resource_deadline;
    char *prepared_sql;
    bool prepared_ready, dedicated;
    MYSQL *my;
    PGconn *pg;
    MYSQL_STMT *stmt;
    PGresult *pg_pending;
    json_t *columns, *pending;
    uint64_t affected, insert_id;
    atomic_bool cancelled;
    uint64_t queries, rows;
} DbSession;
uint64_t db_now(void);
void db_error(DbError *e, const char *code, const char *message);
void db_driver_error(DbSession *s, DbError *e, const char *stage);
bool db_config(json_t *input, DbConfig *out, DbError *e);
DbSession *db_acquire(json_t *input, const char *scope, const char *request, DbError *e);
void db_pin(DbSession *s);
void db_release(DbSession *s);
void db_discard(DbSession *s);
void db_shutdown(void);
void db_scope_end(const char *scope);
void db_cancel(const char *request);
void db_operation_begin(const char *request);
void db_operation_end(void);
void db_record(uint64_t elapsed, const DbError *error, json_t *result);
json_t *db_stats(void);
bool db_connect(DbSession *s, DbError *e);
bool db_control(DbSession *s, const char *sql, DbError *e);
bool db_start(DbSession *s, const char *sql, json_t *params, json_t *options, DbError *e);
json_t *db_fetch(DbSession *s, unsigned limit, DbError *e);
bool db_next_result(DbSession *s, DbError *e);
void db_result_close(DbSession *s);
void db_result_finish(DbSession *s);
bool db_reset(DbSession *s);
json_t *db_dispatch(const char *name, json_t *args, const char *scope, const char *request,
                    DbError *e);
#endif
