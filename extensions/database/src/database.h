#ifndef HHY_DATABASE_H
#define HHY_DATABASE_H

#include <jansson.h>
#include <stdbool.h>
#include <stddef.h>

json_t *database_ping(const char *url, const char **error);
json_t *database_query(const char *url, const char *sql, json_t *params,
                       size_t maximum_rows, const char **error);
json_t *database_execute(const char *url, const char *sql, json_t *params,
                         const char **error);
json_t *database_transaction(const char *url, json_t *statements,
                             const char **error);

#endif
