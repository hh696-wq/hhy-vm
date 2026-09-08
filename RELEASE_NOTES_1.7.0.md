# HHY 1.7.0 — Optimizing compiler and verified runtime experiments

This release preserves the language and the default direct Bytecode path. Optional optimizations are independently switchable and observable.

- Whole-program structured HIR, callable CFG regions, independent verification, and six verified passes: folding, constant/copy propagation, peephole, unreachable code, and DCE.
- Bounded integer register MIR with parameter type feedback, guarded specialization, unboxed intermediates, and arithmetic deoptimization at the original source operation.
- Scalar replacement for a single directly indexed local integer List. Original managed allocation reservations remain to preserve quota and GC behavior; this is not physical allocation elision.
- Runtime instruction selection, call layouts, exception/unwind tables, frame pooling, and conditional map inline cache from the v1.6 implementation train.
- Differential execution, verifier mutation, sanitizer, cancellation, allocation and per-platform performance evidence.

Enable the compiler with `HHY_COMPILER=ir`. Enable typed MIR with `HHY_FEEDBACK_SPECIALIZATION=1`, and eligible List scalar replacement with `HHY_SCALAR_REPLACEMENT=1`. All remain opt-in. Unsupported types, captures, effects, or escape shapes use generic execution. Performance admission is workload- and platform-specific; synthetic speedups do not establish general gains.

See [COMPILER_IR.md](COMPILER_IR.md) for representation, budgets, switches and limitations. Database retains its independent version and requires HHY 1.5.0 or later.
