# 26. HHY Language Status Report · 2026-09-07

Published status of HHY semantics, Runtime, performance, and engineering quality with reproducible CI measurements.

## 26.1 HHY 1.7.0 implementation and compatibility

1.7.0 passes local tests, four-platform CI and release validation. Language specification 1.0 remains frozen. Official sample, HTML and macOS/Linux Database 1.0.0 extensions are verified compatible. VS Code 0.2.0 and Sublime 0.1.0 retain independent versions; syntax and editor contract checks pass.


HIR, six optimization passes, integer MIR, type-feedback guards/deopt and local List scalar replacement are implemented. Explicitly enable HHY_COMPILER=ir, HHY_FEEDBACK_SPECIALIZATION=1 and HHY_SCALAR_REPLACEMENT=1; direct Bytecode remains default. Real workloads have not met default-enablement benefit gates. Scalar replacement retains original GC/quota reservations.


[Optimization switches, verification and supported scope](/docs/COMPILER_IR.md)

v1.7.0 implementation


[1.7.0 release notes](/docs/RELEASE_NOTES_1.7.0.md)

Released capabilities and limits


## 26.2 Release summary

{% hint style="info" %}
As of 2026-09-08, HHY v1.7.0 delivers Database 1.0.0 and scoped resources alongside the complete Web Runtime: embedding, HTTP/1.1, Router, middleware, uploads, streaming, SSE, multiple workers, and observability. v1.4.0–v1.4.2 are capability milestones shipped together in v1.4.3. Bytecode remains default; AST remains the semantic oracle and explicit fallback.
{% endhint %}


| Dimension | Question | Current conclusion |
| --- | --- | --- |
| Language baseline | Are core semantics stable? | Pipe, Value, Stream, Error, and core callable contracts are frozen |
| Runtime health | Are resource, memory, and cancellation boundaries reliable? | Resource limits, GC stress, sanitizers, fuzzing, and explicit ownership governance are present |
| Performance | Is performance measurable and controlled? | Fixed workloads, five-sample medians, machine-readable evidence, and blocking budgets are established |
| Engineering governance | Are changes auditable? | Four-platform CI, layered gates, version consistency, and release evidence form a closed loop |


## 26.3 Data at a glance

| Signal | Result | Evidence basis |
| --- | --- | --- |
| Current formal release | v1.7.0 | Four platform archives, per-asset SHA-256, and SHA256SUMS |
| Execution engines | Bytecode default / AST fallback | Full dual-engine suite and machine-readable decision |
| Core callables | 121 | Runtime Callable Contract Registry |
| Continuous-verification platforms | 4 | macOS arm64, Linux arm64, Linux x86_64, Windows x86_64 |
| v1.3.10 CI engine baseline | Historical measurement passed | 1M CPU 0.3695; short task 1.0088; sustained JSON 1.0207 |
| v1.3.10 Profiler overhead | 1.0269× / +2.786 ms | Historical nine samples; limits 1.35× and 12 ms |
| Bytecode cache | Not admitted | Five workloads × 21 samples; compile+verify is not a major cost |
| Complete practical projects | 6 | AST/Bytecode end-to-end acceptance with stable exit status |


## 26.4 v1.5.0 / Database 1.0.0 release acceptance

Released 2026-09-07. Dual-database/engine, TLS, scope cleanup, cancellation, CMS database lifecycle and cross-platform release gates pass. Local million-row streaming and five-minute-per-backend sustained reports are available. RDS and 24-hour soak validation remain pending.


[Read DB 1.0.0 acceptance](/docs/DATABASE_1.0.0_ACCEPTANCE.md)

HHY 1.5.0 · Database 1.0.0


[Read HHY 1.5.0 release notes](/docs/RELEASE_NOTES_1.5.0.md)

HHY 1.5.0 · Database 1.0.0


## 26.5 v1.4.3 Web Runtime and release verification

| Check | Result | Measurement scope |
| --- | --- | --- |
| Embedded Runtime | 100,000 repeated calls | One loaded context with AST/Bytecode differential output checks |
| HTTP stress verification | 1,000,000 requests · zero failures | Loopback on the release-validation host, 16 concurrent clients |
| HTTP duration and throughput | 131.439 seconds · 7,608.1 requests/second | Host-specific short-request workload, not a public-network or business-service throughput claim |
| HTTP regression | End-to-end coverage | Router, Query, 404/405/413/500, recovery, middleware, static caching, upload cleanup, CORS, signed cookies, and gzip |
| Streaming and operations | End-to-end coverage | Stream, SSE, Range, metrics, health, multiple workers, and development reload |


{% hint style="info" %}
This section cites v1.4.3 release-validation records, not a new benchmark run on 2026-09-04. CPU, Profiler, cache, and six-runtime comparisons below retain their original v1.3.10 versions and methods.
{% endhint %}


[Read the v1.4.3 release-validation record](/docs/RELEASE_NOTES_1.4.3.md)

Site-hosted release notes with test scale, results, and capability boundaries.


## 26.6 Current capability boundaries

| Area | Current boundary |
| --- | --- |
| HTTP serving | Runtime provides HTTP/1.1; Caddy, Nginx, or a load balancer handles TLS and HTTP/2 |
| WebSocket | Outside the v1.4 capability boundary |
| Execution engines | Bytecode default with AST fallback; no JIT, process/disk Bytecode cache, or unverified external Bytecode loading |
| Native integration | C embedding uses opaque handles and a JSON ABI; it does not expose Runtime internals or a third-party Native Extension ABI |


## 26.7 Overall baseline and compatibility

| Baseline | Stable commitment | Verification |
| --- | --- | --- |
| Language semantics | No second Pipe, Stream, or Error model | Specification examples, Parser/Checker fixtures, and valid-program regression |
| Callable contracts | Names, arity, effect, lazy, cancellable, and threading metadata are machine-readable | Contract Registry JSON and current callable-contract consistency checks |
| Diagnostics | CLI text and JSON/LSP share the Core checking path | Diagnostic schema and LSP protocol tests |
| Extension boundary | Third-party capabilities prefer the Process Extension Protocol | Manifest integrity, Protocol 1, and official-extension acceptance |
| C embedding | Public opaque Application/Context handles and JSON ABI; Runtime internals remain private | One-time loading, repeated calls, request isolation, and AST/Bytecode differential checks |


The current formal baseline is v1.7.0. Bytecode is default; the AST evaluator remains the semantic oracle and is selectable through --engine ast or HHY_ENGINE=ast. Compiler-produced Stream Kernels must pass their independent Verifier; dynamic and unknown shapes fall back losslessly to general Bytecode.


## 26.8 v1.2.2 release and extension status

| Capability | Current state | Acceptance result |
| --- | --- | --- |
| Extension distribution | Ed25519-signed Registry and deterministic resolution | Tampering, unknown sources, and dependency conflicts fail closed |
| Reproducible environment | Lockfile and content-addressed offline cache | The same lock yields the same graph and rebuilds offline |
| Safe change | Transactional install, upgrade, and explicit rollback | Failed upgrades preserve the old environment |
| HTML 0.2.0 | Lexbor, CSS selectors, and single-parse multi-field projection | Malformed HTML, hard limits, truncation, and structured errors pass on four platforms |
| Protocol decision | Retain the bounded synchronous batch API | HTML retains synchronous batches; DB 1.0 uses scoped handles, cancellation and host cursor Streams; generic Stream credit remains unavailable |


{% hint style="info" %}
The v1.2.2 release contains macOS arm64, Linux x86_64, Linux arm64, and Windows x86_64 archives, per-package SHA-256 files, and a combined SHA256SUMS. The HTML extension remains effect = none and performs no independent file, network, or subprocess access.
{% endhint %}


[Open the HHY Language v1.2.2 release](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.2.2)

Download all four platform archives and checksums and read the release notes.


## 26.9 v1.3.7–v1.3.10 Bytecode hardening

| Version | Core delivery | Verified conclusion |
| --- | --- | --- |
| v1.3.7 | Named specialization metadata, unified stack/error rules, fallback reasons | No magic kinds; three-path differential and metamorphic gates pass |
| v1.3.8 | Compiler-produced versioned Stream Kernel IR | Runtime does not inspect AST shapes; independent verification; safe dynamic fallback |
| v1.3.9 | Shared optimization decisions for normal and profiled execution | Kernel/opcode, cancellation, CPU/Heap attribution, and machine reports remain consistent |
| v1.3.10 | Performance-triggered cache governance | Evidence did not admit a cache; no process/disk cache; unverified external Bytecode rejected |


{% hint style="info" %}
Each version completed implementation, Release/Debug tests, sanitizers, fuzzing, measured performance, four-platform Actions, formal Release, and Homebrew Formula verification before the next version began.
{% endhint %}


[Open the HHY Language v1.7.0 release](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.7.0)

Four platform archives, per-asset SHA-256, SHA256SUMS, and Web Runtime release notes.


## 26.10 Historical performance baseline · v1.3.10

Final v1.3.10 CI evidence comes from commit 4ddc8c3 on GitHub Actions Ubuntu 24.04: schema-2 paired/interleaved engine benchmarks plus independent Profiler and cache-decision artifacts. Ratios are Bytecode/AST wall time; lower than one means Bytecode is faster.


| Gate | Measured | Limit | Result |
| --- | --- | --- | --- |
| 1M CPU | 0.3695× | 0.90× | Pass |
| Short task | 1.0088× | 1.25× | Pass |
| Sustained JSON/I/O | 1.0207× | 1.10× | Pass |
| Profiler overhead | 1.0269× / +2.786 ms | 1.35× / +12 ms | Pass |


| Cache-admission workload | Compile+verify median | Cold-run median | Share |
| --- | --- | --- | --- |
| Hello | 0.0097 ms | 5.860 ms | 0.1649% |
| Advanced Flow | 0.0269 ms | 5.916 ms | 0.4546% |
| Stdlib | 0.0286 ms | 7.003 ms | 0.4086% |
| Sustained JSON | 0.0089 ms | 33.518 ms | 0.0266% |
| Core Flow 1M | 0.0105 ms | 166.285 ms | 0.0063% |


{% hint style="info" %}
The joint cache threshold is compile+verify ≥ 1 ms and ≥ 20% of cold-run wall time. Every workload remains far below it. Even assuming zero cache-read cost, reproducible benefit is absent, so v1.3.10 implements no process or disk cache and continues to reject unverified external Bytecode.
{% endhint %}


## 26.11 v1.3.10 six-runtime same-machine rerun

Rerun on 2026-09-01 on macOS 26.6.2 arm64 with HHY 1.3.10, PHP 8.5.10, Go 1.27.0, Python 3.14.7, Lua 5.5.1, and OpenJDK 26.0.2.1. The fixed task maps, filters, stable-distincts, materializes, and counts one million integers; all six implementations validate output 333334. After two warmups, two independent rounds use seven deterministically shuffled and interleaved fresh processes per runtime. Wall time includes process startup; Go and Java are precompiled and compile time is excluded.


| Implementation | Version | Round 1 median | Round 2 median | Combined range |
| --- | --- | --- | --- | --- |
| Go | 1.27.0 | 7.995 ms | 7.969 ms | 7.622–15.325 ms |
| Lua | 5.5.1 | 18.368 ms | 17.957 ms | 17.582–19.822 ms |
| PHP | 8.5.10 | 43.174 ms | 43.549 ms | 42.588–49.033 ms |
| Java | OpenJDK 26.0.2.1 | 49.394 ms | 48.153 ms | 46.995–53.066 ms |
| HHY Bytecode | 1.3.10 | 55.297 ms | 53.404 ms | 51.027–78.702 ms |
| Python | 3.14.7 | 81.747 ms | 86.459 ms | 79.993–87.774 ms |


| Comparison | Round 1 | Round 2 | Interpretation |
| --- | --- | --- | --- |
| HHY / PHP | 1.28× | 1.23× | HHY uses about 23%–28% more wall time on this task |
| HHY / Java | 1.12× | 1.11× | Java is slightly faster including fresh-JVM startup |
| HHY / Python | 0.68× | 0.62× | HHY takes less wall time on this task |
| HHY / Lua | 3.01× | 2.97× | Lua is faster on this integer loop |
| HHY / Go | 6.92× | 6.70× | Precompiled Go remains substantially faster |


{% hint style="info" %}
This is one CPU/materialization workload, not a general language ranking. Each implementation uses an idiomatic loop and distinct container; Java uses HashSet/ArrayList and includes fresh-JVM startup. All 84 timed samples, versions, orders, and sources are retained locally under performance-analysis/2026-09-01-v1.3.10-language-comparison/, which project policy excludes from GitHub.
{% endhint %}


## 26.12 Governance conclusion and watch list

- Overall status: v1.7.0 / DB 1.0.0 passed local, CI and cross-platform release gates; real RDS and 24-hour soak validation remain pending.
- Web conclusion: embedding, HTTP, Router, middleware, uploads, Stream/SSE, multi-worker serving, and observability ship together.
- Engine policy: Bytecode is default; AST remains permanently available as the semantic oracle, differential-test engine, and --engine ast emergency fallback.
- Performance conclusion: one million Web short requests completed with zero failures; existing CPU, short-task, sustained-JSON, and Profiler gates continue to pass.
- Cache conclusion: compile+verify is not a major cold-run cost; no cache is introduced until new data triggers review.
- Update rule: synchronize this report whenever the release baseline, measurement method, engine/cache decision, or overall risk conclusion changes.


[Open v1.3.11 continuous-verification evidence](https://github.com/hh696-wq/hhy-vm/actions/runs/33581452558)

Four-platform builds, sanitizers, fuzzing, performance gates, and practical-project acceptance.
