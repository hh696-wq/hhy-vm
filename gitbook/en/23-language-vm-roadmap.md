# 23. Language and VM Evolution Roadmap

v1.2.0 officially ships the signed extension-distribution path, followed by locking, offline installs, and safe rollback.

## 23.1 Current release and two future stages

{% hint style="info" %}
v1.2.0 is officially released, adding an Ed25519-signed Registry, transitive resolution, dry runs, and transaction-safe installation while preserving local extension compatibility, with macOS arm64, Linux x86_64, Linux arm64, and Windows x86_64 MSYS2 archives.
{% endhint %}


{% hint style="info" %}
View the interactive diagram for this section on [hhylang.dev](https://hhylang.dev/en/learn/language-vm-roadmap).
{% endhint %}


## 23.2 Release lineage, timing, and acceptance gates

| Release | Recommended window | Primary delivery | Required before the next stage |
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
| v1.2.1 · Planned | After v1.2.0 acceptance | Locking, offline installs, and safe rollback | The same lock produces the same graph; offline rebuilds work; failed upgrades preserve the old environment |
| v2.0 · Conditional | After sufficient ecosystem evidence | Ecosystem opening and ABI decision | At least two real integrations prove the process protocol insufficient; otherwise retain the process protocol and do not publish a Native ABI |


Note: these dates are recommended windows, not release commitments.


## 23.3 Evolution principles

| Principle | Constraint |
| --- | --- |
| Freeze semantics first | Stabilize Pipe, Value, Stream, Error, and cancellation semantics before broadening the ecosystem surface |
| Usable and measurable before fast | Every capability needs deterministic errors, resource bounds, and cross-platform tests before optimization |
| Protocol first | Integrate third-party capability through the Process Extension Protocol instead of inventing a second language model |
| ABI only when justified | Evaluate a Native ABI only after Runtime stabilization and measured need; choosing not to publish one is a valid result |


Review the roadmap once per quarter. Only unfrozen releases may move; scheduling changes must not weaken published semantics, compatibility commitments, or migration paths.


## 23.4 Explicit non-commitments

- No second Pipe, Stream, or Error model merely to justify a release number.
- No public exposure of internal Runtime C structures without a compatibility strategy.
- No use of recommended windows as a reason to skip testing, security, or cross-platform validation.
- No simultaneous rush into a remote registry, Native ABI, and multiple official extensions before stage gates pass.
