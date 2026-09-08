# 22. Database Extension Guide

Database 1.0.0: MySQL/PostgreSQL pools, remote TLS, transactions, prepared statements, cursors, exact types and HHY Streams. Requires HHY 1.5.0 or newer; compatibility with 1.7.0 is verified.

## 22.1 Version and installation

{% hint style="info" %}
DB 1.0.0 shipped with HHY 1.5.0 on 2026-09-07 and is independently versioned. DB supports macOS arm64 and Linux arm64/x86_64; Windows Runtime archives exclude DB.
{% endhint %}


[Download Database 1.0.0](https://github.com/hh696-wq/hhy-vm/releases/tag/database-v1.0.0)

HHY 1.5.0 · Database 1.0.0


```sh
hhy --version
# Extract the matching release archive and preserve bin/ and lib/
hhy install ./hhy-database-1.0.0-darwin-arm64
hhy list

# Or build from the repository root
make -C extensions/database
./build/hhy install ./extensions/database
```


## 22.2 Data sources and remote TLS

**config.local.json**

```text
{
  "driver": "mysql",
  "host": "127.0.0.1",
  "port": 3306,
  "user": "cms_test",
  "password": "CHANGE_ME",
  "database": "hhy_test",
  "allow": [
    "127.0.0.1:3306"
  ],
  "tls": "disabled",
  "max_open": 4,
  "max_idle": 2,
  "connect_ms": 5000,
  "acquire_ms": 1000,
  "timeout_ms": 30000
}
```


{% hint style="info" %}
This example targets a dedicated local fixture. Protect the config file and exclude it from Git; read production credentials from the environment or a secret source. Remote sources require a map with the real host, exact host:port in allow, verify_identity TLS and a CA path. Deployment owns VPC, allowlists and database grants; real RDS acceptance remains pending.
{% endhint %}


Legacy URLs remain supported for loopback. Use maps for remote endpoints and pool options. MySQL uses ? and PostgreSQL uses $1, $2; identifiers cannot be value parameters. The allow list is trusted-script policy, not an OS sandbox.


## 22.3 Pools and queries

**query.hhy**

```hhy
import database
let cfg = read_text(path(args[0])) |> parse_json
database.query(cfg, "SELECT ? AS value", [42], { typed: true }) |> print
database.stats() |> print
```


```sh
hhy run query.hhy ./config.local.json
```


Pools are isolated by data source and extension process. Defaults are max_open=4 and max_idle=2; add worker budgets together. Idle/lifetime retirement, resets, acquire timeouts and metrics are supported. Duration options such as timeout_ms: 5s work directly. Results default to String/null; typed mode preserves numeric types, exact envelopes preserve decimals/large integers and binary maps to BytesBuffer.


## 22.4 Transactions and streaming

{% hint style="info" %}
The examples require an existing pages(id, title) fixture; replace placeholders for PostgreSQL. Transaction callbacks commit on success and roll back on error. MySQL DDL is not a multi-step atomic transaction; installers must track migration checkpoints.
{% endhint %}


**transaction.hhy**

```hhy
import database
let cfg = read_text(path(args[0])) |> parse_json
cfg |> database.with_transaction { tx ->
    database.execute(tx, "UPDATE pages SET title = ? WHERE id = ?", ["Hello HHY", 1])
    database.query(tx, "SELECT id, title FROM pages WHERE id = ?", [1])
} |> print
```


**stream.hhy**

```hhy
import database
let cfg = read_text(path(args[0])) |> parse_json
database.stream(cfg, "SELECT id, title FROM pages ORDER BY id", [])
    |> take(100)
    |> collect
    |> print
```


Streams pull batches; take closes results early. Handles cannot cross requests/workers or survive close; request completion cleans abandoned resources. Unread results can discard a physical connection, so early stream closure inside a transaction may invalidate that transaction.


## 22.5 API and limits

| API | Purpose |
| --- | --- |
| ping / query / execute | Connectivity, queries and writes |
| connection / begin / commit / rollback / close | Scoped connections and transactions |
| with_transaction / savepoint / rollback_to / release_savepoint | Transaction callbacks and savepoints |
| prepare / query_prepared / execute_prepared | Reuse server statements on one connection |
| transaction / execute_many | Atomic batches of 1–100 DML statements |
| cursor / fetch / next_result / close_result / stream | Incremental results; next_result supports MySQL |
| stats / capabilities | Metrics and version capabilities |


{% hint style="info" %}
Fields are limited to 64 KiB and protocol messages to 1 MiB. Bounded query returns at most 10000 rows and reports truncation. Saturation returns DB_POOL_TIMEOUT; cancellation/timeouts may discard connections. DB_COMMIT_UNKNOWN means the commit outcome is unknown: never automatically replay writes.
{% endhint %}


## 22.6 Acceptance and installation scenarios

MySQL 8.4/PostgreSQL 17 pass local/CI, AST/Bytecode, two-worker, TLS positive/negative, cancellation and sanitizer acceptance, plus local million-row streaming. An install.hhy database fixture covers failure recovery, repeat installation, failed upgrade rollback and backup restoration; it is not a full CMS. Local sustained tests ran five minutes per backend, not a 24-hour soak or RDS failover certification.


[Full API and migration guide](/docs/DATABASE_1.0.0_README.md)

HHY 1.5.0 · Database 1.0.0


[Measurements and reproduction](/docs/DATABASE_1.0.0_ACCEPTANCE.md)

HHY 1.5.0 · Database 1.0.0


[View passing release Actions](https://github.com/hh696-wq/hhy-vm/actions/runs/34079510695)

HHY 1.5.0 · Database 1.0.0
