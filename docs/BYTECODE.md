# HHY Bytecode

HHY v1.7.0 uses the verified Bytecode engine by default. The AST evaluator
remains the permanent semantic oracle and explicit fallback through
`--engine ast` or `HHY_ENGINE=ast`.

## Current boundary

- Parser, Checker, and Resolver are shared by AST and Bytecode execution.
- Every AST node lowers to an explicit Opcode with source location, child and
  subtree shape, constant reference, and resolved local-slot metadata.
- `src/bytecode_runtime.h` is the only Runtime-facing preparation boundary.
  The Runtime cannot bypass compilation, the Verifier, or execution-plan
  preparation; the internal boundary is versioned.
- The Verifier rejects unknown opcodes, invalid constants, malformed trees,
  excessive nesting, non-canonical `HALT`, invalid slot metadata, invalid
  Stream Kernel type/stack flow, and instructions following terminal return.
- Execution planning verifies bounded instruction, frame, and operand shape
  before evaluation.
- `hhy run` and `hhy profile` default to Bytecode. `--engine ast` keeps the
  complete AST oracle available for rollback and differential testing.
- `hhy bytecode <file.hhy>` compiles, verifies, and disassembles the internal
  IR without executing it.
- `hhy bytecode --metrics <file.hhy>` emits schema-1 JSON containing source and
  Chunk size plus compile+verify and verify+prepare timings.

## Compiler and Runtime additions in v1.7.0

The default remains the direct AST-to-Bytecode compiler. `HHY_COMPILER=ir`
selects structured HIR, independently verified passes, and the Bytecode backend.
`HHY_FEEDBACK_SPECIALIZATION=1` enables bounded integer register MIR;
`HHY_SCALAR_REPLACEMENT=1` additionally permits eligible local List scalar
replacement. MIR is also available with the direct Bytecode compiler; it does
not require changing the engine to AST. All three options remain opt-in.

HIR is not the legacy Stream Kernel IR: it represents the whole program before
Bytecode emission. MIR covers eligible straight-line integer expressions.
Every enabled pass is followed by verification, and emitted Bytecode still
passes its own verifier. Unsupported shapes use generic execution. List scalar
replacement retains managed allocation reservations and is not heap allocation
elision. There is no SSA/phi, JIT, or machine-code backend.

The v1.6 call-layout, exception-table, frame-pool, call-unwind, and Map-cache
experiments were delivered with 1.7.0; they remain independently opt-in.
Correctness fixes to host boundaries and file cleanup are always active.
See [usage and rollback](CURRENT_VERSION.md), [Compiler IR](architecture/COMPILER_IR.md),
and [Runtime call rules](architecture/VM_CALL_RUNTIME.md).

## Stream specialization hardening

- v1.3.7 replaced magic specialization kinds with named operations and central
  metadata for stack effects, errors, eligibility, and stable fallback reasons.
- v1.3.8 moved expression-shape recognition from Runtime to Compiler and emits
  versioned Stream Kernel IR. The Runtime executes only verified kernels;
  dynamic or unknown closures use general Bytecode without semantic loss.
- Every specialization is disassemblable and independently verifiable.
  AST/general-Bytecode/specialized-Bytecode differential and metamorphic tests,
  damaged-plan tests, and fuzzing cover the three paths.
- v1.3.9 made normal execution and Profiler share the same optimization
  decision. Profile JSON schema 2 records selection reasons, kernel version,
  opcodes, CPU attribution, Heap attribution, cancellation, and dropped-event
  counts without changing the selected semantic path.

## Cache governance in v1.3.10

The conditional cache capability was measured before implementation. Five
fixed real workloads use 21 paired fresh processes each. On the 2026-09-01
macOS arm64 measurement, compile+verify medians were 0.004–0.012 ms and only
0.0078%–0.1341% of cold Bytecode-run wall time. They did not meet the joint
admission threshold of at least 1 ms and at least 20% of cold wall time.

Therefore v1.3.10 deliberately provides no process cache, no disk `.hhyc`
cache, and no external precompiled-Bytecode loader. Unverified external
artifacts are rejected. The machine-readable decision gate fails CI if future
measurements cross the threshold, forcing an explicit design and threat-model
review instead of silently enabling a cache.

Any future cache must bind a complete fingerprint covering source SHA-256,
recursive dependency SHA-256, HHY version, Bytecode and Stream Kernel format,
compiler features, target triple, and security-policy version. A hit must still
pass checksum validation, bounded decoding, the full Bytecode Verifier, and the
execution-plan verifier. Stale, damaged, unknown, truncated, bit-flipped, or
incompatible data must be ignored and safely recompiled. Storage must use
atomic writes, a hard size bound, and no untrusted symlink traversal.

## Verification and release evidence

- Complete fixtures and practical workloads run independently under AST and
  Bytecode; stdout, stderr, exit status, diagnostics, resource limits, and
  cancellation are compared.
- Debug ASan/UBSan, Release suites, GC stress, deterministic and coverage-guided
  fuzzing, profiler overhead, performance budgets, and cache-admission evidence
  are blocking gates.
- v1.3.7, v1.3.8, v1.3.9, and v1.3.10 were released sequentially with macOS
  arm64, Linux arm64, Linux x86_64, and Windows x86_64 archives, per-asset
  SHA-256 files, SHA256SUMS, and successful tag evidence.

Opcode numeric values, the in-memory Chunk layout, the Runtime boundary, and
Stream Kernel representation remain internal implementation details, not a
public C or external Bytecode ABI.
