# Database release history

## 1.0.0 — stable API release

- Replace per-call short connections with bounded per-data-source connection pools.
- Add explicit remote endpoint authorization, structured configuration and TLS policies.
- Add scoped connections, read/write transactions, savepoints, reusable prepared
  statements, batch execution, incremental cursors and MySQL result chains.
- Add exact scalar/decimal/binary results, column metadata, duplicate-column handling,
  generated IDs, parameter/row/field/message budgets and operation metrics.
- Add cancellation, resource leases, request cleanup and HHY 1.5.0 host integration
  for transaction callbacks, Duration/BytesBuffer values, lazy Streams and prefork isolation.
- Add MySQL/PostgreSQL integration, TLS negative cases, protocol, sanitizer, streaming
  and Web resource tests. RDS test configuration is deferred by the project owner.
- Preserve the four legacy URL APIs and String/null results, with explicit bounded
  query truncation and safer error categories for operational failures.

The stable tag must only be published after the release commit passes its Actions
checks. This release does not claim completion of a 24-hour soak or actual
RDS failover testing. See README.md for precise compatibility and resource limits.

## 0.2.0

Local MySQL/PostgreSQL parameterized ping/query/execute and batch DML transactions.
