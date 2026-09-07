# HHY Database 1.0

Database access for MySQL and PostgreSQL with explicit remote endpoint authorization,
TLS identity verification, bounded connection pools, scoped read/write transactions,
savepoints, reusable statements, batch writes, incremental cursors, typed/binary
values, MySQL multiple results and operational metrics.

Requires HHY 1.5.0 or later. The accompanying Runtime adds transaction callbacks,
lazy database Streams, Duration/BytesBuffer conversion, request-scope cleanup,
cancellation and fork-safe extension processes.

Downloads target macOS arm64 and Linux arm64/x86_64. Extract the matching archive,
verify its SHA-256, and install its directory with `hhy install`. Keep bin and lib
together. Full API and limits are in the included README.

The release workflow gates publication on MySQL 8.4/PostgreSQL 17 integration,
AST/Bytecode and Web isolation, TLS/cancellation/resource tests, streaming evidence,
sanitizer tests and the normal HHY release suites. Consult the associated Actions
artifacts for actual tested versions, duration and results. RDS validation is
pending the owner's dedicated instance; a 24-hour deployment soak is not claimed.
