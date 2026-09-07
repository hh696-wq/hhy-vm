# HHY 1.5.0 — Database host integration

This runtime release accompanies the independently versioned database
extension 1.0. It adds scoped extension resources, fork-safe process restart,
cancellation propagation, database transaction callbacks, native Duration/binary
conversion and lazy database Streams. Web and embedded-call boundaries release
leaked database resources on success and error.

Existing database URL calls remain available. Remote data sources require explicit
configuration and endpoint authorization. Database release notes, API limits and
verification details are in extensions/database/README.md and CHANGELOG.md.

Database packages target macOS arm64 and Linux arm64/x86_64. Windows keeps the
existing runtime/sample/HTML release scope. Real RDS acceptance awaits the owner's
test configuration; local/CI success is not a claim of cloud failover certification.
