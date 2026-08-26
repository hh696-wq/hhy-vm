# database process extension

This C11 package implements Protocol v1 callables for PostgreSQL and MySQL:

- `database.ping(url)`
- `database.query(url, sql, params, max_rows?)`
- `database.execute(url, sql, params)`
- `database.transaction(url, statements)`

PostgreSQL uses `$1` placeholders. MySQL uses `?`. Both paths use their native
parameter APIs and never concatenate values into SQL. Result values are strings
or null so database precision is preserved.

```sh
make -C extensions/database
build/hhy install extensions/database
```

```hhy
import database

let url = require_env("DATABASE_URL")
database.ping(url) |> print
database.query(url, "SELECT id, name FROM users WHERE active = $1", [true], 100)
    |> get("rows")
    |> print
```

The first transaction API executes 1–100 parameterized `INSERT`, `UPDATE`, or
`DELETE` statements on one connection and commits them atomically. Any failed
statement rolls the whole list back:

```hhy
database.transaction(url, [
    { sql: "INSERT INTO users (id, name) VALUES (?, ?)", params: [1, "HHY"] },
    { sql: "UPDATE users SET name = ? WHERE id = ?", params: ["HHY VM", 1] }
])
```

MySQL uses `?` placeholders and PostgreSQL uses `$1`, `$2`, and so on. DDL and
queries are intentionally excluded from this first transaction version.

The Runtime passes only explicitly supplied arguments over the protocol. It does
not copy the host environment into the extension process.
