# 19. CLI Reference

Run, check, format, use the REPL, inspect dry-run plans, and profile performance.

## 19.1 Version and release identity

Use --version to confirm the binary version, project author, open-source license, and official contact details. Source builds use ./build/hhy; release archives or PATH installations can invoke hhy directly.


### HHY · Version information

```console
$ ./build/hhy --version
hhy 1.2.0
© 2026 HHY Language contributors
Author: houhuiyang
License: Apache License 2.0
https://hhylang.dev/
huiyang.hou@qq.com
```

Actual HHY 1.2.0 command output. The CLI reports the version, author, license, website, and contact address directly.


{% hint style="info" %}
From an official archive, run ./bin/hhy --version in the extracted directory. After make install or adding HHY to PATH, run hhy --version.
{% endhint %}


## 19.2 Complete command set

```sh
hhy script.hhy [args...]
hhy run script.hhy [args...]
hhy repl
hhy check script.hhy...
hhy fmt script.hhy...
hhy fmt --check script.hhy...
hhy ast script.hhy
hhy tokens script.hhy
hhy run --dry-run script.hhy
hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
hhy profile script.hhy [args...]
hhy profile --cpu script.hhy
hhy profile --heap --format json --output profile.json script.hhy
hhy --version
hhy --help
```


| Command | Purpose |
| --- | --- |
| hhy run | Run a script and pass args |
| hhy profile | Analyze CPU hotspots, call counts, and managed-Heap allocations |
| hhy repl | Start the interactive environment |
| hhy check | Check syntax and core semantics |
| hhy fmt | Write canonical formatting |
| hhy fmt --check | Check formatting only |
| hhy ast | Print the AST |
| hhy tokens | Print Lexer tokens |
| hhy run --dry-run | Preview a redacted execution plan |


hhy script.hhy is shorthand for hhy run script.hhy. Use -- after Runtime options when script arguments may begin with a dash.


## 19.3 CPU and Heap profiling

profile executes the script and collects CPU and managed-Heap data in the same run by default. Reports go to stderr, leaving script stdout unchanged, and the command preserves the script's exit code.


```sh
hhy profile examples/09-profile-algorithms.hhy -- fibonacci 20
hhy profile --cpu examples/09-profile-algorithms.hhy fibonacci 20
hhy profile --heap --format json --output profile.json examples/09-profile-algorithms.hhy fibonacci 20
```


| Option | Behavior |
| --- | --- |
| --cpu | Collect only 1ms process-CPU samples and call counts |
| --heap | Collect only cumulative allocations, allocation count, Heap peak, and post-GC usage |
| --format text\|json | Select a human- or machine-readable report; default: text |
| --output <path> | Write the report to a file instead of stderr |
| --limit NAME=VALUE | Override Runtime resource limits, as with run |
| --dry-run | Block external effects, as with run, and profile plan execution |


```console
$ hhy profile examples/09-profile-algorithms.hhy -- fibonacci 20
HHY profile: examples/09-profile-algorithms.hhy

Summary
  Wall time        0.006 s
  CPU time         0.004 s
  CPU utilization  64.5%
  CPU samples      2
  Heap peak        755.9 KiB
  Heap after GC    4.0 KiB
  Allocated        523.9 KiB
  Allocations      11107

CPU hotspots
  CPU%    Samples      Calls  Function
  100.0%        2      21891  fibonacci  examples/09-profile-algorithms.hhy:5:1

Allocation hotspots
  Bytes          Objects  Function
  515.6 KiB        10966  fibonacci  examples/09-profile-algorithms.hhy:5:1

fibonacci 6765
```


{% hint style="info" %}
CPU profiling samples process CPU time, so file, HTTP, and process waits are not misreported as CPU hotspots. Scripts that finish in a few milliseconds may need a larger or repeated workload. Heap metrics cover memory managed by HHY's Boehm GC, not extension subprocesses or memory owned directly by native libraries.
{% endhint %}


## 19.4 Interpreter performance evolution

v1.2.0 keeps the AST Interpreter and reduces function-call cost through resolution, static slots, and reusable lightweight frames. Deterministic locals use the Slot fast path, while closure captures, globals, and builtins retain the compatible Env path.


{% hint style="info" %}
View the interactive diagram for this section on [hhylang.dev](https://hhylang.dev/en/learn/cli-reference).
{% endhint %}


{% hint style="info" %}
The Bytecode VM is a data-driven future direction, not a prerequisite today. HHY enters that phase only when profiling shows AST dispatch has become the dominant remaining hotspot.
{% endhint %}


## 19.5 Runtime resource limits

The run command accepts repeatable --limit NAME=VALUE options. Sizes require b/kb/mb/gb/kib/mib/gib, durations require ns/us/ms/s/min/h, and counts have no unit.


```sh
hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
```


| Limit | Default |
| --- | --- |
| max_memory | 512mib |
| max_open_files | 256 |
| max_processes | 16 |
| max_parallelism | 16 |
| max_http_body | 16mib |
| max_regex_steps | 1000000 |
| max_recursion | 256 |
| max_runtime | 0 (no total CLI limit) |


## 19.6 Stable exit codes

```text
0  success
1  unhandled runtime error
2  syntax or static-check error
3  invalid CLI usage
4  file I/O, process, or network error
5  timeout or cancellation
```


Automation should branch on stable exit codes rather than error text.
