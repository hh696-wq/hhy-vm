# HHY Runtime governance

This document defines the internal module and ownership rules introduced in
HHY 1.1.8 and extended through 1.7.0 (reviewed 2026-09-09). It does not add a public C ABI and does not change HHY semantics.

## Module boundaries

| Owner | Responsibility | May depend on |
| --- | --- | --- |
| `runtime_limits.c` | Default resource-limit policy | Public Runtime value types only |
| `runtime.c` evaluator | AST execution, environments, values and callable dispatch | Limits, contracts, profiler and platform libraries |
| `src/bytecode.c` | Direct compilation, Bytecode verification, and compiler selection | Compiler and internal Bytecode metadata |
| `src/compiler.c` | Structured HIR, pass pipeline, independent verification and Bytecode emission | Versioned compiler/Bytecode interfaces |
| `src/typed_plan.c` | Bounded integer MIR construction and verification | Internal Bytecode and typed-plan definitions |
| `src/bytecode_runtime.h` | Runtime-facing preparation boundary | Verified compiler/Bytecode execution plans |
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

## Optimization and evidence rules

The default direct Bytecode compiler and AST oracle remain available. Compiler
passes run before execution and never bypass verification; Runtime executes
verified plans. The research prototype in `compiler/ir.c` does not implement the
production whole-program compiler. See [Compiler IR](architecture/COMPILER_IR.md).

Experimental call layouts, exception tables, frame pooling, call unwind, and Map
slot caches are opt-in. Host argument conversion and native-file cleanup fixes
remain unconditional. See [call ownership and unwind](architecture/VM_CALL_RUNTIME.md),
[cache lifetime rules](architecture/VM_INLINE_CACHE.md), and [GC diagnostics](architecture/VM_GC_SCHEDULER.md).

An optimization must retain Error source/order, effect counts, cancellation,
managed quotas, and GC roots. List scalar replacement retains allocation
reservations; it must not be reported as physical heap allocation removal.
Feature-specific validation and budgets complement the original `make quality`
gate; passing that target alone does not admit a new optimization by default.

Evidence records commit, binary digest, platform, input, switches, raw paired
samples, and both correctness and resource results. Each platform is assessed
independently. A skip, absent production budget, or historical local measurement
cannot stand in for a passing current release gate. See the
[current guide](CURRENT_VERSION.md) for commands and rollback.
