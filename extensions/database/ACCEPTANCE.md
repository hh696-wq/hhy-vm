# DB 1.0.0 acceptance record

Date: 2026-09-07. Minimum host: HHY 1.5.0. The release workflow must
pass against the final release commit before publishing packages.

| Area | Evidence | Scope |
| --- | --- | --- |
| Pool, transaction, savepoint, prepared statement, batch, cursor, exact types | `tests/integration.py` | Real MySQL 8.4 and PostgreSQL 17 |
| Native Duration and BytesBuffer, lazy Stream, transaction callback | `tests/runtime.py` | AST and Bytecode |
| Worker connection isolation, abandoned transaction cleanup, extension crash recovery | `tests/runtime.py` | Two prefork Web workers; worker survives extension SIGKILL |
| TLS verified identity, wrong identity, missing CA, cancellation, field limit and lease expiry | `tests/robustness.py` | Both databases with dedicated local test CA |
| Install interruption/recovery, repeated installation, failed upgrade, logical backup/restore | `tests/cms.py` | Minimal install.hhy fixture on both engines and databases; not a full CMS |
| Protocol framing and resource rejection | `tests/protocol.py` | Offline subprocess tests |
| Memory safety | CI ASan/UBSan integration | Linux includes leak detection; macOS local run excludes unsupported leak detector |
| Distribution | Release workflow | macOS arm64, Linux arm64/x86_64; relocated installation and checksums |

Local macOS sustained pool test, 300 seconds per database:

| Database | Successful SELECT iterations | Open connections at finish | Pinned / in use at finish |
| --- | ---: | ---: | ---: |
| MySQL 8.4.11 | 1,583,501 | 1 | 0 / 0 |
| PostgreSQL 17.11 | 2,752,244 | 1 | 0 / 0 |

Fault-injection operations intentionally return errors before the timed loop.
This is a local single-client test, not a comparison against Go, Python or PHP.

Local incremental-result test (64-byte payload plus an ID):

| Database | 100,000 rows | 1,000,000 rows | Extension peak RSS, small / large |
| --- | ---: | ---: | ---: |
| MySQL | 0.210 s | 1.961 s | 14,024,704 / 14,254,080 bytes |
| PostgreSQL | 0.172 s | 1.585 s | 23,707,648 / 23,937,024 bytes |

Measurements are environment-specific. CI uploads its own actual results as
`database-evidence-<commit>`; streaming uses 10,000 and 100,000 rows there.
Reproduce using the scripts above with `--config <private-config.json>`;
the configuration is a JSON list of the data source maps documented in README.
For the larger stream test add `--rows 1000000`; for sustained testing add
`--ca <fixture-ca.pem> --soak-seconds 300` to robustness.py.

RDS test credentials are explicitly deferred by the owner. Actual RDS networking,
authentication and failover have not been certified. A 24-hour soak is not claimed.
The README lists driver-specific cancellation, precision and resource limitations.
