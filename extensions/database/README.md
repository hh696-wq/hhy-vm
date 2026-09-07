# HHY Database 1.0

MySQL and PostgreSQL access for HHY CLI scripts and persistent Web workers. Requires
HHY **1.4.4 or later** for scoped resources, transaction callbacks, native binary
values, cancellation and database Streams. The extension is independently
versioned; see `hhy.toml` and `CHANGELOG.md` for the exact release status.

## Build and install

```sh
make -C extensions/database
build/hhy install --yes extensions/database
```

Build dependencies: a C11 compiler, Jansson, libcurl, OpenSSL, libpq, MySQL client
headers/libraries and pthreads. Released database binaries target macOS arm64 and
Linux x86_64/arm64. The Windows HHY runtime archive does not contain this extension.
Use the checksummed database release archive or the copy bundled with HHY; preserve
its `bin/` and `lib/` directories. Read the included third-party notices.

## Configure a data source

```hhy
import database

let config = {
    driver: "mysql",
    host: "127.0.0.1",
    port: 3306,
    database: "company_site",
    user: "cms",
    password: require_env("CMS_DB_PASSWORD"),
    allow: ["127.0.0.1:3306"],
    tls: "disabled",
    max_open: 4,
    max_idle: 2,
    acquire_ms: 1s,
    connect_ms: 5s,
    timeout_ms: 30s
}

database.query(config, "SELECT id, title FROM pages WHERE published = ? LIMIT 20", [true])
    |> get("rows")
    |> print
```

For RDS or another remote database, set its real host and port, authorize the exact
`host:port` endpoint in `allow`, and use `tls: "verify_identity"` with the provider's
CA certificate in `ca`. IPv6 hosts are unbracketed in `host` and bracketed in the
allow-list endpoint, for example `[::1]:3306`. Network access, database grants and
RDS whitelist/VPC configuration are still deployment responsibilities. RDS-specific
failover/credential-rotation acceptance awaits a dedicated test instance; local
and CI tests must not be described as RDS production certification.

The manifest declares configured network endpoints and explicitly supplied CA
files. Authorization is enforced against the trusted script's data-source `allow`
list, not an OS network sandbox. Never accept database configuration, credentials,
SQL, or `allow` directly from an HTTP request. DNS addresses may change while the
configured hostname remains the same; TLS identity verification authenticates the
server after resolution. Passwords and SQL parameters are never included in
extension errors, pool metrics or protocol diagnostics.

The legacy URL API remains supported for loopback hosts, including percent-encoded
credentials and IPv6. Use a structured map for remote addresses and options; URL
query options are rejected rather than silently ignored. MySQL URLs default to
3306 and PostgreSQL URLs to 5432. PostgreSQL parameters use `$1`, `$2`, etc.; MySQL
uses `?`. Identifiers cannot be bound as value parameters.

### Configuration contract

| Field | Default | Range / semantics |
| --- | --- | --- |
| driver | mysql | mysql / postgresql / postgres |
| host, user, database | required | Explicit strings; NUL rejected |
| password | empty | Read from a secret source in the script |
| port | 3306 / 5432 | Integer 1–65535 |
| allow | required for maps | Exact endpoint list, up to 32 entries |
| tls | verify_identity | disabled / required / verify_ca / verify_identity |
| ca | client default | Explicit CA file; incorrect verification never falls back to plaintext |
| max_open | 4 | 1–32 per data source and extension process |
| max_idle | 2 | 0–32, no greater than max_open |
| idle_ms | 60000 | 1–3600000 |
| lifetime_ms | 1800000 | 1–86400000 |
| connect_ms | 5000 | 100–60000; MySQL native connect/socket settings have second granularity |
| acquire_ms | 1000 | 0–60000; pool wait is cancellable |
| timeout_ms | 30000 | 100–300000; operation and transaction/cursor lease budget |

HHY Duration values are converted to milliseconds for database arguments, so
`timeout_ms: 5s` is valid. Unknown options, wrong types and inconsistent limits are
rejected. MySQL sessions use utf8mb4; both backends initialize UTC and reset session
state before reuse. MariaDB client builds support identity verification but reject
verify_ca-only mode, because their verification interface differs from Oracle's.

## API

| Callable | Result and behavior |
| --- | --- |
| ping(source) | Bounded SELECT 1 connectivity check |
| query(source, sql, params, max_rows_or_options?) | rows, columns, count, truncated, done, row_count, last_insert_id, ok |
| execute(source, sql, params) | Non-row statement; affected rows and generated ID |
| connection(source) | Scoped dedicated connection; close explicitly |
| begin(source, options?) | Scoped transaction handle |
| commit(tx), rollback(tx) | End transaction; invalidate ordinary transaction handle |
| with_transaction(source, options?, callback) | Host callback; commit on success, roll back on failure |
| savepoint(tx, name), rollback_to(tx, name), release_savepoint(tx, name) | Validated savepoint identifier, same physical connection |
| transaction(source, statements) | Compatible atomic batch of 1–100 INSERT/UPDATE/DELETE statements |
| execute_many(source, sql, parameter_rows) | Atomic parameterized batch of 1–100 executions |
| prepare(source, sql) | Scoped reusable statement; prepared on first execution, reused thereafter |
| query_prepared(statement, params, options?), execute_prepared(statement, params, options?) | Reuse the server statement on the dedicated connection |
| cursor(source, sql, params, options?) | Scoped incremental result cursor |
| fetch(cursor, batch_rows?) | rows, columns, done, row_count, last_insert_id; default batch 100 |
| next_result(cursor) | Advance a fully consumed MySQL result; false at end |
| close_result(resource) | Release current result; retain an owning transaction/dedicated/prepared resource |
| close(resource) | Close resource; roll back outstanding transaction |
| stream(source, sql, params, options?) | Lazy HHY Stream with bounded fetch and early-close cleanup |
| stats() | Aggregate pool, wait, operation, error, cancellation and row counters; no data-source credentials |
| capabilities() | Implemented protocol/API capabilities and exact extension version |

A source is a trusted configuration map, supported legacy URL, or a scoped resource
handle. Resource tokens are random, owned by the extension process and bound to the
host request scope. They are not durable IDs: never serialize them to a database,
share them between requests, transfer them between workers, or use them after close.
Open resources expire at their lease deadline. Connection/statement leases use
lifetime_ms; transaction/cursor leases retain their initial operation deadline.

### Transactions

```hhy
import database

let config = require_env("CMS_DATABASE_CONFIG") |> parse_json
let result = config |> database.with_transaction { tx ->
    let current = database.query(tx, "SELECT balance FROM accounts WHERE id = ? FOR UPDATE", [1])
    database.execute(tx, "UPDATE accounts SET balance = balance - ? WHERE id = ?", [10, 1])
    current.rows
}
print(result)
```

Named functions may also be passed to with_transaction. Trailing callback syntax
is HHY pipeline syntax, not `database.with_transaction(config) { ... }`.

Transaction options are `isolation` (READ COMMITTED, REPEATABLE READ, SERIALIZABLE,
READ UNCOMMITTED) and `read_only` (Bool). Server support determines which combinations
are accepted. Query/execute inside transactions permit SELECT/INSERT/UPDATE/DELETE/
WITH, while controls use the dedicated transaction methods. PostgreSQL additionally
supports CALL/FETCH/MOVE/CLOSE in explicit transactions. MySQL DDL and procedures
that could implicitly commit are excluded from transaction-scoped SQL. Savepoints
are not independently committing nested transactions. Transaction callbacks must
consume their streams before returning.

Use the same transaction handle for every operation. Calling the ordinary pool
from inside a transaction can exhaust a small pool and has different transaction
semantics. Commit failure due to connection loss/timeout reports DB_COMMIT_UNKNOWN:
it is not proof of rollback. Never blindly repeat writes after an uncertain result.
The extension does not automatically retry application SQL.

### Results, types and limits

Default result values retain the legacy String/null contract. Query/cursor options:
`typed: true`, `positional: true`, and a shorter `timeout_ms`. Native scalar parameters
retain their numeric/boolean binding types. Null is distinct from an empty string.

Typed results use HHY Int/Float/Bool when representable. Decimal, oversized integer,
non-finite float, date, time, timestamp and JSON values have explicit `type`/`value`
representations so precision and semantics are not silently lost. Timestamp text
retains the backend's timezone information, if any; sessions start in UTC.

Native HHY BytesBuffer parameters and binary results cross the JSON protocol as
bounded hexadecimal type envelopes. Direct protocol clients see
`{"type":"bytes","value":"00ff"}`; HHY converts this to BytesBuffer. Other tagged
parameters accept decimal/integer/date/time/timestamp/json strings. Column metadata
preserves native type codes; portable code must not assume MySQL and PostgreSQL type
numbers are interchangeable. Positional rows preserve duplicate column names;
map rows reject duplicates instead of silently overwriting them.

MySQL row_count retains CLIENT_FOUND_ROWS semantics (matching rows for UPDATE).
last_insert_id belongs to the same MySQL execution, never a separate pooled query.
PostgreSQL applications obtain generated values through SQL RETURNING. Unsigned
values beyond HHY Int range are returned without precision loss as integer envelopes.

Safety limits: 64 total native sessions per extension, 8 active protocol workers,
64 queued calls, 1024 parameters, 256 result columns, 64 KiB per field, approximately
128 KiB decoded/256 KiB encoded per row or batch, 1 MiB per protocol message. Buffered
query returns at most 10000 rows and can truncate earlier at the byte budget;
inspect truncated or use stream/cursor. A partially consumed standalone result is
not reused as an idle connection. Early termination inside an owning transaction
can invalidate that transaction, requiring rollback of the enclosing operation.
Streaming memory depends on batch size and the largest incoming driver row, not
the full result size. libpq can receive a large row before the field limit is checked.

### Stored procedures and multiple results

Use MySQL cursor + fetch + next_result for every CALL result, including a final
status-only result. A dedicated connection can preserve session variables for
INOUT/OUT parameter workflows; close_result retains that connection. PostgreSQL
CALL output is a normal row result, and transaction-scoped refcursors can be fetched
explicitly. next_result is not supported for PostgreSQL extended-query mode; it
returns DB_NOT_SUPPORTED rather than pretending to expose a MySQL-style result chain.

## Lifetime, concurrency and cancellation

Each HHY Web worker lazily starts its own extension after fork. Parent pipes and
sockets are not shared by children. Each extension serves up to eight concurrent
protocol requests and owns its bounded pools. A single HHY call remains synchronous;
Web concurrency comes from workers. Database calls are not advertised as transferable
HHY parallel-closure work. Size the total budget across instances, workers, extension
processes and data sources, including rolling-deployment overlap.

Web requests and embedded hhy_call scopes close leaked resources on both success
and failure. A disconnected Web client, Runtime cancellation, or database deadline
requests database-side cancellation. MySQL uses a separate short-lived control
connection to kill its own running query; libpq 17+ uses asynchronous cancellation,
older libpq uses its legacy cancellation API. Control connections count in server
capacity planning in addition to pooled data connections. Failed cancellation can
only guarantee connection disposal, not immediate completion of server-side work.
Unknown write/commit outcomes must be reconciled by the application.

A poisoned or unfinished connection is destroyed. Healthy idle connections are
reset, lazily retired at idle/lifetime deadlines and reconnected on a later call.
No user query is replayed after an error. Pool saturation has bounded waiting and
explicit DB_POOL_TIMEOUT / DB_QUEUE_FULL errors.

## Installation and migration tools

The driver provides parameterized DML, native DDL, transactions and schema queries.
CMS install.hhy owns migration versions, table prefixes, administrator creation,
configuration files and installation locking. MySQL DDL is not an all-or-nothing
multi-step transaction: record each completed migration and validate before retry.
Never implement an installer by concatenating request fields into SQL identifiers.
A CMS application itself is not included in this database-extension release.

## Verification

```sh
python3 extensions/database/tests/protocol.py
python3 extensions/database/tests/integration.py --config /path/to/test-configs.json
python3 extensions/database/tests/runtime.py --config /path/to/test-configs.json
python3 extensions/database/tests/robustness.py --config /path/to/test-configs.json --ca /path/to/test-ca.pem --soak-seconds 60
python3 extensions/database/tests/streaming.py --config /path/to/test-configs.json --rows 1000000
```

Test configs are a JSON list of dedicated test data sources. Integration fixtures
create/drop `hhy_db_acceptance`, `hhy_runtime_test`, `hhy_scope_test` and
`hhy_db_proc`; never run them against a business database. GitHub Actions provisions
MySQL 8.4/PostgreSQL 17, configures test TLS, verifies both HHY engines and prefork
isolation, and uploads sanitizer, robustness and streaming evidence. Manual Actions
runs support up to 7200 seconds of sustained queries per backend. A 24-hour soak and
real RDS failover certification are separate, still-required deployment validations.
