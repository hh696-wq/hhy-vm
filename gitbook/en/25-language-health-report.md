# 25. HHY Language Status Report · 2026-08-31

Published status of HHY semantics, Runtime, performance, and engineering quality with reproducible CI measurements.

## 25.1 Release summary

{% hint style="info" %}
The current HHY language baseline is v1.1.8. Core semantics remain stable, Runtime resource and ownership governance is established, structured diagnostics and editor tooling are operational, all four continuous-verification platforms are green, and no performance budget was triggered in this report.
{% endhint %}


| Dimension | Question | Current conclusion |
| --- | --- | --- |
| Language baseline | Are core semantics stable? | Pipe, Value, Stream, Error, and core callable contracts are frozen |
| Runtime health | Are resource, memory, and cancellation boundaries reliable? | Resource limits, GC stress, sanitizers, fuzzing, and explicit ownership governance are present |
| Performance | Is performance measurable and controlled? | Fixed workloads, five-sample medians, machine-readable evidence, and blocking budgets are established |
| Engineering governance | Are changes auditable? | Four-platform CI, layered gates, version consistency, and release evidence form a closed loop |


## 25.2 Data at a glance

| Signal | Result | Evidence basis |
| --- | --- | --- |
| Core callables | 96 | Runtime Callable Contract Registry |
| Website HHY examples | 47 passing | Parser/Checker validation on every production build |
| Complete specification code blocks | 21 passing | docs/HHY_V1.md documentation check |
| Continuous-verification platforms | 4 | macOS arm64, Linux arm64, Linux x86_64, Windows x86_64 |
| Performance workloads | 4 / 20 samples | Five consecutive samples per workload, reported by median |
| Complete practical projects | 6 | End-to-end acceptance with stable exit status |


## 25.3 Overall baseline and compatibility

| Baseline | Stable commitment | Verification |
| --- | --- | --- |
| Language semantics | No second Pipe, Stream, or Error model | Specification examples, Parser/Checker fixtures, and valid-program regression |
| Callable contracts | Names, arity, effect, lazy, cancellable, and threading metadata are machine-readable | Contract Registry JSON and 96-contract consistency checks |
| Diagnostics | CLI text and JSON/LSP share the Core checking path | Diagnostic schema and LSP protocol tests |
| Extension boundary | Third-party capabilities prefer the Process Extension Protocol | Manifest integrity, Protocol 1, and official-extension acceptance |
| C ABI | Runtime internals are not currently a public ABI | Reconsider only when real integrations prove the process protocol insufficient |


The latest sampling point is v1.1.8. Releases v1.1.6–v1.1.8 provide the testing, editor, and Runtime-governance capabilities supporting this report; they are not three separate language-wide reports.


## 25.4 Measured performance

Environment: GitHub Actions Ubuntu 24.04, Linux 6.17 Azure x86_64, 4 vCPUs, Python 3.12.3; commit 43db191, HHY v1.1.8. Each workload ran five consecutive times; values are milliseconds.


| Workload | Median | Min–max | Absolute budget | Budget used |
| --- | --- | --- | --- | --- |
| CLI --version | 4.001 ms | 3.948–5.065 ms | 100 ms | 4.0% |
| Basic Flow | 5.480 ms | 5.363–5.585 ms | 250 ms | 2.2% |
| Core Flow 100k | 45.020 ms | 44.297–46.172 ms | 500 ms | 9.0% |
| JSON Flow | 5.543 ms | 5.492–6.255 ms | 250 ms | 2.2% |


| Relative signal | Measured | Gate | Result |
| --- | --- | --- | --- |
| Basic Flow / CLI startup | 1.37× | ≤ 12× | Pass |
| Core Flow 100k / CLI startup | 11.25× | ≤ 20× | Pass |
| JSON Flow / CLI startup | 1.39× | ≤ 12× | Pass |


{% hint style="info" %}
These numbers track HHY regressions; they are not a cross-language ranking claim. Raw samples are retained in the corresponding GitHub Actions performance-baseline artifact. Budget changes require data and an explanation.
{% endhint %}


## 25.5 Governance conclusion and watch list

- Overall status: language semantics are stable; Runtime and tooling are in evidence-driven gradual governance.
- Primary current risk: Runtime remains concentrated, so future extraction must move one stable responsibility at a time.
- Performance policy: preserve semantics and resource boundaries first, then optimize from Profiler and benchmark evidence.
- Ecosystem policy: improve process-extension tooling before making any Native ABI commitment.
- Update rule: revise this report when the baseline, measurement method, or overall risk conclusion changes—not by copying it for every patch release.


[Open continuous verification evidence](https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml)

Four-platform builds, sanitizers, GC stress, fuzzing, performance baselines, practical projects, and release archives.
