# HHY Runtime governance

This document defines the internal module and ownership rules introduced in
HHY 1.1.8. It does not add a public C ABI and does not change HHY semantics.

## Module boundaries

| Owner | Responsibility | May depend on |
| --- | --- | --- |
| `runtime_limits.c` | Default resource-limit policy | Public Runtime value types only |
| `runtime.c` evaluator | AST execution, environments, values and callable dispatch | Limits, contracts, profiler and platform libraries |
| Runtime owner | Stream, module, cleanup and root Runtime teardown | Resource-specific close/free functions |
| `profiler.c` | Measurement and profile serialization | Public profiler types |

Future extraction follows one stable responsibility at a time: Value/Map,
Stream, codecs, effectful resources, then evaluator ownership. A move must keep
fixtures, exit codes, diagnostics and resource ceilings unchanged.

## Ownership vocabulary

Internal C boundaries use the annotations in `src/runtime_ownership.h`:

- `HHY_BORROWED`: the callee does not release or outlive the owner;
- `HHY_MANAGED_SCANNED`: BDWGC storage that may retain managed pointers;
- `HHY_MANAGED_ATOMIC`: BDWGC byte storage containing no managed pointers;
- `HHY_NATIVE_OWNED`: native or library storage with explicit teardown.

Managed `Value` arrays must use scanned storage. Native resources are released
by their owning resource path, while the Runtime owner closes remaining streams,
cleanup records and modules exactly once during teardown.

## Enforced gates

`make quality` checks the module location and ownership annotations, then runs a
reviewed benchmark budget. `make test` compares normal and `HHY_GC_STRESS=1`
outputs. `make test-debug` runs ASan/UBSan; CI additionally runs native Release,
GC stress, fuzzing, practical projects and archive verification.

Changing `benchmarks/performance-budget.json` is an explicit policy change. It
must include benchmark evidence and a written explanation; ordinary Runtime
changes update evidence, not the budget.
