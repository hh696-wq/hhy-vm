# HHY v1.7.0 Language Health Report

Updated 2026-09-09 from repository code, policy files, and archived release evidence.
This update reconciles documentation; it is not a fresh benchmark or a new CI run.
Core is 1.7.0, the frozen language specification is 1.0.0, and Database retains its
independent 1.0.0 version. This report and the public specifications are tracked; raw performance-analysis
artifacts are retained separately from the Core repository.

## Current release and admission status

| Area | Recorded result | Boundary |
| --- | --- | --- |
| Release | 1.7.0 tag at `8fb0b8f71892ec281256027530f065d7de5921af`; final release workflow `34199886300` succeeded | Based on the 2026-09-08 local release archive; not re-queried in this documentation update |
| Correctness | Local suites, four-platform CI `34197497786`, and Database CI `34197497779` passed | A passing correctness matrix is separate from performance admission |
| Compiler | Structured HIR, six passes, integer MIR, feedback guards/deopt, and eligible List scalar replacement delivered | Direct Bytecode remains default; new optimization switches are opt-in |
| Runtime | v1.6 call/exception/frame/cache experiments delivered with 1.7.0 | No independent v1.6.x release; no superinstruction, new GC, or async scheduler admitted |
| Resources | Scalar replacement preserves managed allocation reservations | No claim of physical allocation elision or memory savings |
| Compatibility | Official sample/HTML and supported macOS/Linux Database packages validated | Third-party extensions were not individually validated; Windows archive excludes Database |
| Deployment | Web and Database integration evidence exists | Real RDS and 24-hour soak remain unverified |

Source: [final release record](https://github.com/hh696-wq/hhy-vm/actions/runs/34199886300),
[cross-platform budgets](../benchmarks/vm-compiler-ir-policy.json),
and [compiler policy](../benchmarks/vm-compiler-ir-policy.json).
The policy records `default_admitted: false`: real workloads did not meet the
5% runtime improvement gate. Synthetic gains do not establish general speedups.
See [current usage](CURRENT_VERSION.md) and [known limitations](KNOWN_LIMITATIONS.md).

## Historical Bytecode baseline (v1.3.x)

The ratios below are retained historical evidence, not fresh 1.7.0 measurements.

- Runtime error values are fully constructed before publication; the original
  invalid-regex fuzz input is retained as a permanent regression corpus entry.
- Bytecode is the default engine; AST remains the semantic oracle and explicit
  `--engine ast` / `HHY_ENGINE=ast` fallback.
- Historical CI Bytecode/AST wall-time ratios: 1M CPU `0.3695`, short task `1.0088`,
  sustained JSON `1.0207`.
- Profiler overhead: `1.0269x`, or `+2.786 ms`, below the `1.35x` and `12 ms`
  limits.
- Cache governance: no cache admitted. Across five workloads and 21 paired
  samples, compile+verify remained below both the `1 ms` and `20%` joint
  thresholds. There is no process cache, disk `.hhyc` cache, or unverified
  external Bytecode loader.

Historical CI evidence: [run 33581452558](https://github.com/hh696-wq/hhy-vm/actions/runs/33581452558).

## Historical six-runtime fresh-process rerun (2026-09-01)

Environment: macOS 26.6.2 arm64. Fixed workload: generate `0..999999`, multiply
by two, retain values divisible by three, stable-distinct, materialize, and
count. Every implementation returned `333334`. Each round used two warmups and
seven timed fresh processes per runtime with deterministic shuffled interleaving.
Go and Java compilation was excluded; runtime startup was included.

| Runtime | Version | Round 1 median | Round 2 median | Combined range |
| --- | --- | ---: | ---: | ---: |
| Go | 1.27.0 | 7.995 ms | 7.969 ms | 7.622–15.325 ms |
| Lua | 5.5.1 | 18.368 ms | 17.957 ms | 17.582–19.822 ms |
| PHP | 8.5.10 | 43.174 ms | 43.549 ms | 42.588–49.033 ms |
| Java | OpenJDK 26.0.2.1 | 49.394 ms | 48.153 ms | 46.995–53.066 ms |
| HHY | 1.3.10 | 55.297 ms | 53.404 ms | 51.027–78.702 ms |
| Python | 3.14.7 | 81.747 ms | 86.459 ms | 79.993–87.774 ms |

HHY/PHP was `1.28x` and `1.23x`; HHY/Java `1.12x` and `1.11x`;
HHY/Python `0.68x` and `0.62x`; HHY/Lua `3.01x` and `2.97x`; HHY/Go
`6.92x` and `6.70x`.

This is a single CPU/materialization workload, not a general language ranking.
All 84 samples, runtime versions, execution orders, and source programs are in
the original comparison archive（本地历史归档，未随源码分发）.
