# 21. Database Extension Guide

Install the official database 0.2.0 extension, configure MySQL/PostgreSQL with JSON, and run queries, writes, and transactions.

## 21.1 What the database extension is

{% hint style="info" %}
database 0.2.0 ships with HHY v1.1.0 and is a real C11 process extension in the repository. It currently supports MySQL, PostgreSQL, parameterized reads, parameterized writes, and the first transaction API. Connection handles, pooling, streaming queries, and complete database type mapping are future work.
{% endhint %}


| Callable | Purpose | Current boundary |
| --- | --- | --- |
| database.ping(url) | Validate connectivity and return database information | Creates a short-lived connection per call |
| database.query(url, sql, params, max_rows?) | Run a bounded parameterized query | Result contains columns and rows |
| database.execute(url, sql, params) | Run a parameterized write or controlled DDL | Returns affected-row information |
| database.transaction(url, statements) | Atomically run 1–100 writes | INSERT/UPDATE/DELETE only; rolls back on failure |


## 21.2 Install the extension and its four callables

```sh
make -C extensions/database
./build/hhy install ./extensions/database
./build/hhy list
```


install validates hhy.toml, the HHY version range, the extension command, and SHA-256 integrity, then displays network capabilities before confirmation. After installation, import database starts the isolated extension process, completes the Protocol 1 handshake, and registers all four callables.


## 21.3 Connection configuration and credential safety

```sh
cd extensions/database/examples/hhy_extension_test
cp config.example.json config.local.json
chmod 600 config.local.json
```


**config.local.json**

```text
{
  "url": "mysql://root:CHANGE_ME@127.0.0.1:3306/hhy_extension_test",
  "database": "hhy_extension_test",
  "max_rows": 1000
}
```


| Driver | Connection URL example | Parameter placeholder |
| --- | --- | --- |
| MySQL | mysql://user:password@127.0.0.1:3306/hhy_extension_test | ? |
| PostgreSQL | postgresql://user:password@127.0.0.1:5432/hhy_extension_test | $1, $2, … |


{% hint style="info" %}
Replace CHANGE_ME with the local password. config.local.json is ignored by Git in the example directory; never put real credentials in .hhy source, documentation, or Git. The examples also require the database field to equal hhy_extension_test so they cannot target an application database accidentally.
{% endhint %}


## 21.4 Project one: inspect the test database read-only

```sh
./build/hhy run \
  extensions/database/examples/hhy_extension_test/read.hhy \
  extensions/database/examples/hhy_extension_test/config.local.json
```


**read.hhy**

```hhy
import database
import { load_database_config } from "./lib/config.hhy"

let config = load_database_config(args[0])
let result = database.query(
    config.url,
    "SELECT COUNT(*) AS table_count FROM information_schema.TABLES WHERE TABLE_SCHEMA = ? AND TABLE_TYPE = 'BASE TABLE'",
    [config.database],
    1
)

print("Database", config.database)
print("Table count", result.rows[0].table_count)
```


The complete read.hhy in the repository also lists every table's name, storage engine, and estimated row count. SQL values go through the driver's prepared-statement parameter API and are never concatenated into the query text.


## 21.5 Project two: controlled writes and transactions

```sh
./build/hhy run extensions/database/examples/hhy_extension_test/write-demo.hhy \
  extensions/database/examples/hhy_extension_test/config.local.json --write

./build/hhy run extensions/database/examples/hhy_extension_test/transaction.hhy \
  extensions/database/examples/hhy_extension_test/config.local.json --write
```


**transaction-example.hhy**

```hhy
database.transaction(config.url, [
    { sql: "INSERT INTO _hhy_transaction_test (id, message) VALUES (?, ?)", params: [1, "created"] },
    { sql: "UPDATE _hhy_transaction_test SET message = ? WHERE id = ?", params: ["committed", 1] }
]) |> print
```


{% hint style="info" %}
Both write examples require an explicit --write flag and touch only dedicated temporary tables inside hhy_extension_test. transaction rejects SELECT and DDL; if any statement fails, the extension rolls back the entire transaction.
{% endhint %}


## 21.6 Current boundaries and troubleshooting

| Symptom | Check |
| --- | --- |
| ModuleNotFoundError | Run install first and use hhy list to confirm database 0.2.0 is installed |
| cannot open .../read.hhy | Run the complete path from the repository root; the directory is named hhy_extension_test |
| Connection failure | Check the service, port, user, password, database name, and local network scope declared by hhy.toml |
| SQL parameter error | MySQL uses ?; PostgreSQL uses $1, $2, …; identifiers cannot be value parameters |


[Continue with the extension system internals](/en/learn/extensions-roadmap)

Learn about manifest validation, capability declarations, process loading, the Protocol 1 handshake, and extension-author constraints.
