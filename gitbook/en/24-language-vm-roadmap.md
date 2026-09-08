# 24. Language and VM Evolution Roadmap

v1.7.0 delivers Database 1.0.0, pooling, remote TLS and Runtime resource integration while retaining Web Runtime and both AST/Bytecode engines.

## 24.1 HHY 1.7.0 implementation and compatibility

1.7.0 passes local tests, four-platform CI and release validation. Language specification 1.0 remains frozen. Official sample, HTML and macOS/Linux Database 1.0.0 extensions are verified compatible. VS Code 0.2.0 and Sublime 0.1.0 retain independent versions; syntax and editor contract checks pass.


HIR, six optimization passes, integer MIR, type-feedback guards/deopt and local List scalar replacement are implemented. Explicitly enable HHY_COMPILER=ir, HHY_FEEDBACK_SPECIALIZATION=1 and HHY_SCALAR_REPLACEMENT=1; direct Bytecode remains default. Real workloads have not met default-enablement benefit gates. Scalar replacement retains original GC/quota reservations.


[Optimization switches, verification and supported scope](/docs/COMPILER_IR.md)

v1.7.0 implementation


[1.7.0 release notes](/docs/RELEASE_NOTES_1.7.0.md)

Released capabilities and limits


## 24.2 Language / VM Evolution Roadmap

Current v1.7.0 · Database 1.0.0 released alongside the complete Web Runtime


| Release | Release date | Main changes | Acceptance status |
| --- | --- | --- | --- |
| v1.0.0 · Released | 2026-08-25 | Core language and VM semantics frozen | Pipe, Value, Stream, Error, the core standard library, and three-platform release evidence completed |
| v1.1.0 · Released | 2026-08-26 | Local process extensions and the official database extension | Install/load integrity, synchronous Protocol 1 calls, database 0.2.0, and three-platform release evidence completed |
| v1.1.1 · Released | 2026-08-27 | Performance optimization and resource-boundary stability | hhy profile, interpreter hotspot baselines, and Runtime resource boundaries completed |
| v1.1.2 · Released | 2026-08-27 | HTML extension and static collector framework | Three-platform CI, protocol tests, local fixtures, and the real hhylang.dev crawl completed |
| v1.1.3 · Released | 2026-08-28 | Runtime correctness and performance hardening | GC pressure regression, sanitizers, hash indexes, stable diagnostics, and three-platform release evidence completed |
| v1.1.4 · Released | 2026-08-28 | Safe static spider | URL normalization, discovery, frontier, limits, fingerprint deduplication, and connection-level SSRF protection |
| v1.1.5 · Released | 2026-08-30 | Resumable spider and browser rendering | Persistent frontier, resume, streamed files, optional Playwright, and Windows MSYS2 build evidence |
| v1.1.6 · Completed | 2026-08-31 | Stable engineering baseline | Host capability probes, layered CI, machine-readable performance baselines, and release consistency gates |
| v1.1.7 · Completed | 2026-08-31 | Diagnostics and editor baseline | Versioned JSON diagnostics, Contract Registry JSON, a minimal LSP, and a VS Code editing loop |
| v1.1.8 · Completed | 2026-08-31 | Gradual Runtime governance | First module boundary, internal ownership API, sanitizer/GC stress, and a blocking performance-regression gate |
| v1.2.0 · Released | 2026-08-31 | Official extension distribution and signing | Namespaced identities, Ed25519-signed index and package descriptors, deterministic resolution, dry runs, and transaction-safe installs |
| v1.2.1 · Released | 2026-09-01 | Locking, offline installs, and safe rollback | The same lock produces the same graph; offline rebuilds work; failed upgrades preserve the old environment |
| v1.2.2 · Released | 2026-09-01 | Official HTML complex-extension validation | Real fixtures, observable truncation, structured errors, and four-platform distribution all pass |
| v1.3.0-alpha · Prereleased | 2026-09-01 | Bytecode compiler skeleton | Core syntax compiles, invalid Bytecode is rejected, and AST remains the default engine |
| v1.3.0-beta · Stage gate complete | Merged into v1.3.0 | Bytecode VM execution core | The execution bridge, verifier, resource boundaries, and full dual-engine fixtures passed acceptance |
| v1.3.0-rc · Stage gate complete | Merged into v1.3.0 | Performance, profiler, stack trace, and default-switch gates | Three-platform and failure evidence is complete; the CPU benefit gate failed, so AST remains the default |
| v1.3.0 · Released | 2026-09-01 | Opt-in production Bytecode path | The full suite passes both engines; the performance decision retains AST as default with explicit fallback |
| v1.3.1 · Released | 2026-09-01 | Real-workload compatibility hardening | The official workload dual-engine matrix and capability evidence passed three-platform CI |
| v1.3.2 · Released | 2026-09-01 | VM internal-boundary stabilization | Versioned Bytecode Runtime boundary, static governance, and continuous AST oracle |
| v1.3.3 · Completed | Merged into v1.3.5 | Native Opcode execution | Normal Bytecode execution no longer calls the AST evaluator; full dual-engine semantics pass |
| v1.3.4 · Completed | Merged into v1.3.5 | VM data-path optimization | 1M CPU workload ratio is 0.6805 with no material short-script or JSON/I/O regression |
| v1.3.5 · Released | 2026-09-01 | Bytecode default engine | run, profile, and shorthand default to Bytecode; AST remains the permanent oracle and explicit fallback |
| v1.3.6 · Released | 2026-09-01 | Stream Int fusion performance closure | Provably safe shapes fuse conservatively; unknown shapes fall back losslessly; dual-engine and cross-platform gates pass |
| v1.3.7 · Released | 2026-09-01 | Specialization hardening | Named metadata, unified stack/error rules, fallback reasons, and three-path differential gates pass |
| v1.3.8 · Released | 2026-09-01 | Compiler/Verifier optimization IR | Runtime does not inspect AST shapes; Stream Kernels verify independently and fall back safely |
| v1.3.9 · Released | 2026-09-01 | Profiler and resource consistency | Normal and observed execution share decisions; overhead, cancellation, and Heap-attribution gates pass |
| v1.3.10 · Released | 2026-09-01 | Bytecode cache governance | Five-workload evidence misses admission thresholds; no cache and no unverified external Bytecode |
| v1.3.11 · Released | 2026-09-02 | Runtime error-path hardening | Structured errors publish only after full construction; the original fuzz input is retained as a permanent regression |
| v1.4.0 · Completed | 2026-09-03 | Web Runtime foundation | Embedding, one-time application loading, reusable handlers, and isolation gates complete |
| v1.4.1 · Completed | 2026-09-03 | Web MVP | HTTP/1.1, Router, Query/Header/Cookie, JSON APIs, and stable errors complete |
| v1.4.2 · Completed | 2026-09-03 | Web application capabilities | Middleware, static files, uploads, CORS, signed cookies, gzip, and reload complete |
| v1.4.3 · Released | 2026-09-03 | Streaming and multi-worker | SSE, Stream, large-file Range, processes, and observability complete; one million requests with zero failures |
| v1.5.0 · Released | 2026-09-07 | Database 1.0.0 and Runtime resource integration | Pools, remote TLS, transactions, cursors, exact types, cancellation and worker isolation; dual-database/engine and release Actions pass; RDS validation pending |
| v1.6.0 · Evaluated | 2026-09-08 | Instruction selection and dispatch profiles | Versioned opcode/pair/triple metrics and switches ship in v1.7.0; superinstructions did not meet admission gates |
| v1.6.1 · Implemented | 2026-09-08 | Calls, closures and unwind | Compact call layout, closure paths and exception-region experiments pass differential checks; opt-in, shipped in v1.7.0 |
| v1.6.2 · Evaluated | 2026-09-08 | Conditional inline caches | Map-slot experiment and stable/changing call-site evaluation completed; generic fallback retained, not admitted by default; shipped in v1.7.0 |
| v1.6.3 · Evaluated | 2026-09-08 | GC, memory and scheduling decisions | Allocation/pause and scheduling demand evaluated; no new GC or async scheduler admitted; decisions delivered in v1.7.0 |
| v1.7.0 · Current | 2026-09-08 | Optimizing Compiler & Runtime | Four-platform CI and release validation pass; HIR, six passes, integer MIR, guards/deopt and local List scalar replacement ship opt-in |


## 24.3 Future plans

| Release | Recommended window | Planned delivery | Acceptance gates |
| --- | --- | --- | --- |
| v2.0 · Conditional | After sufficient ecosystem evidence | Ecosystem opening and ABI decision | At least two real integrations prove the process protocol insufficient; otherwise retain the process protocol and do not publish a Native ABI |


Note: these dates are recommended windows, not release commitments.


## 24.4 Evolution principles

| Principle | Constraint |
| --- | --- |
| Freeze semantics first | Stabilize Pipe, Value, Stream, Error, and cancellation semantics before broadening the ecosystem surface |
| Usable and measurable before fast | Every capability needs deterministic errors, resource bounds, and cross-platform tests before optimization |
| Protocol first | Integrate third-party capability through the Process Extension Protocol instead of inventing a second language model |
| ABI only when justified | Evaluate a Native ABI only after Runtime stabilization and measured need; choosing not to publish one is a valid result |


Review the roadmap once per quarter. Only unfrozen releases may move; scheduling changes must not weaken published semantics, compatibility commitments, or migration paths.


## 24.5 Explicit non-commitments

- No second Pipe, Stream, or Error model merely to justify a release number.
- No public exposure of internal Runtime C structures without a compatibility strategy.
- No use of recommended windows as a reason to skip testing, security, or cross-platform validation.
- No simultaneous rush into a remote registry, Native ABI, and multiple official extensions before stage gates pass.
