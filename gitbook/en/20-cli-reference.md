# 20. CLI Reference

Run, check, format, use the REPL, inspect dry-run plans, and profile performance.

## 20.1 HHY 1.7.0 implementation and compatibility

1.7.0 passes local tests, four-platform CI and release validation. Language specification 1.0 remains frozen. Official sample, HTML and macOS/Linux Database 1.0.0 extensions are verified compatible. VS Code 0.2.0 and Sublime 0.1.0 retain independent versions; syntax and editor contract checks pass.


HIR, six optimization passes, integer MIR, type-feedback guards/deopt and local List scalar replacement are implemented. Explicitly enable HHY_COMPILER=ir, HHY_FEEDBACK_SPECIALIZATION=1 and HHY_SCALAR_REPLACEMENT=1; direct Bytecode remains default. Real workloads have not met default-enablement benefit gates. Scalar replacement retains original GC/quota reservations.


[Optimization switches, verification and supported scope](/docs/COMPILER_IR.md)

v1.7.0 implementation


[1.7.0 release notes](/docs/RELEASE_NOTES_1.7.0.md)

Released capabilities and limits


## 20.2 Version and release identity

Use --version to confirm the binary version, project author, open-source license, and official contact details. Source builds use ./build/hhy; release archives or PATH installations can invoke hhy directly.


### HHY · Version information

```console
$ ./build/hhy --version
hhy 1.7.0
© 2026 HHY Language contributors
Author: houhuiyang
License: Apache License 2.0
https://hhylang.dev/
huiyang.hou@qq.com
```

Actual HHY 1.7.0 command output. The CLI reports the version, author, license, website, and contact address directly.


{% hint style="info" %}
From an official archive, run ./bin/hhy --version in the extracted directory. After make install or adding HHY to PATH, run hhy --version.
{% endhint %}


## 20.3 Complete command set

```sh
hhy script.hhy [args...]
hhy run script.hhy [args...]
hhy repl
hhy check script.hhy...
hhy fmt script.hhy...
hhy fmt --check script.hhy...
hhy ast script.hhy
hhy bytecode script.hhy
hhy bytecode --metrics script.hhy
hhy tokens script.hhy
hhy run --dry-run script.hhy
hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
hhy serve app.hhy [args...]
hhy serve --dev app.hhy [args...]
hhy serve --engine ast|bytecode --limit max_memory=256mib app.hhy -- [args...]
hhy profile script.hhy [args...]
hhy profile --cpu script.hhy
hhy profile --heap --format json --output profile.json script.hhy
hhy --version
hhy --help
```


| Command | Purpose |
| --- | --- |
| hhy run | Run a script and pass args |
| hhy run --engine ast\|bytecode | Select the AST fallback or Bytecode engine explicitly |
| hhy serve | Run a persistent Web application configured by web.listen |
| hhy serve --dev | Watch source files and reload the Web application |
| hhy serve --engine ast\|bytecode | Select the execution engine used by Web handlers |
| hhy serve --limit NAME=VALUE | Apply repeatable Web Runtime resource limits |
| hhy profile | Analyze CPU hotspots, call counts, and managed-Heap allocations |
| hhy repl | Start the interactive environment |
| hhy check | Check syntax and core semantics |
| hhy fmt | Write canonical formatting |
| hhy fmt --check | Check formatting only |
| hhy ast | Print the AST |
| hhy bytecode | Compile, verify, and disassemble Bytecode |
| hhy bytecode --metrics | Emit cache-admission compile/verify/prepare metrics as JSON |
| hhy tokens | Print Lexer tokens |
| hhy run --dry-run | Preview a redacted execution plan |


hhy script.hhy is shorthand for hhy run script.hhy. Use -- after Runtime options when script arguments may begin with a dash.


## 20.4 serve · Persistent Web applications

serve loads and checks an HHY Web application, then keeps the process alive. --dev safely reloads after source changes; --engine selects AST or Bytecode; repeatable --limit options bound Runtime resources. HTTP settings such as host, port, max_body, and workers remain explicit in web.listen, while trailing positional values are exposed through read-only args.


```sh
hhy serve app.hhy
hhy serve --dev app.hhy -- 8080
hhy serve --engine bytecode \
  --limit max_memory=256mib \
  --limit max_runtime=30s \
  app.hhy -- 8080 production
```


| Option/argument | Behavior |
| --- | --- |
| --dev | Watch the entry file and imported modules, then reload after changes |
| --engine ast\|bytecode | Select the handler engine explicitly; Bytecode is the default |
| --limit NAME=VALUE | Override memory, runtime, file, process, parallelism, HTTP-body, regex, or recursion limits; repeatable |
| --dry-run | Evaluate the plan while blocking external effects; mainly useful for startup validation |
| -- | End Runtime options and pass every remaining value through args |


{% hint style="info" %}
serve does not guess an application port. The entry file must eventually call web.listen. Production deployments should run behind Caddy, Nginx, or a managed load balancer.
{% endhint %}


## 20.5 Bytecode cache admission evidence

v1.7.0 adds read-only --metrics output for measuring real compile+verify and execution-plan verification cost. Across five fixed workloads and 21 paired fresh-process samples, compile+verify medians were 0.004–0.012 ms, only 0.0078%–0.1341% of cold-run wall time, below the joint 1 ms and 20% thresholds.


```console
$ hhy bytecode --metrics examples/00-hello.hhy
{"bytecode_format_version":1,"compile_verify_ns":9000,"constants":21,"instructions":40,"schema_version":1,"source_bytes":167,"stream_kernel_version":1,"stream_kernels":1,"tool":"hhy bytecode --metrics","verify_prepare_ns":4000}
```


{% hint style="info" %}
No process or disk Bytecode cache is enabled, and third-party precompiled Bytecode remains rejected. Any future admission requires new performance evidence plus a complete source/dependency/version/feature/target/security fingerprint, checksum, bounded decoding, the full Verifier, and execution-plan verification.
{% endhint %}


## 20.6 CPU and Heap profiling

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
$ hhy profile --engine bytecode examples/09-profile-algorithms.hhy -- fibonacci 20
HHY profile: examples/09-profile-algorithms.hhy

Summary
  Engine           bytecode
  Wall time        0.013 s
  CPU time         0.008 s
  CPU utilization  58.8%
  CPU samples      4
  Heap peak        1.8 MiB
  Heap after GC    92.0 KiB
  Allocated        1.7 MiB
  Allocations      33040

CPU hotspots
  CPU%    Samples      Calls  Function
   75.0%        3      21891  fibonacci  examples/09-profile-algorithms.hhy:5:1
   25.0%        1          1  print  examples/09-profile-algorithms.hhy:18:6
    0.0%        0          1  <bytecode-top-level>  examples/09-profile-algorithms.hhy:1:1
    0.0%        0          1  length  examples/09-profile-algorithms.hhy:12:10
    0.0%        0          1  to_int  examples/09-profile-algorithms.hhy:17:19
  Note: fewer than 10 CPU samples; use a larger workload for stable results.

Allocation hotspots
  Bytes          Objects  Function
  1.7 MiB            32856  fibonacci  examples/09-profile-algorithms.hhy:5:1
  11.6 KiB              184  <bytecode-top-level>  examples/09-profile-algorithms.hhy:1:1

fibonacci 6765
```


{% hint style="info" %}
HHY 1.7.0 · 2026-09-08 · macOS arm64 · single local run; fewer than 10 CPU samples, so timings and CPU percentages vary between runs. Default compiler settings; no opt-in optimization flags.
{% endhint %}


{% hint style="info" %}
CPU profiling samples process CPU time, so file, HTTP, and process waits are not misreported as CPU hotspots. Scripts that finish in a few milliseconds may need a larger or repeated workload. Heap metrics cover memory managed by HHY's Boehm GC, not extension subprocesses or memory owned directly by native libraries.
{% endhint %}


## 20.7 Interpreter performance evolution

v1.7.0 defaults to the Compiler/Verifier-validated Bytecode VM. Native Opcode dispatch, static slots, reusable call frames, and low-allocation closure fast paths reduce CPU cost; the AST Interpreter remains the permanent semantic oracle and explicit fallback.


Default: Source → AST → Bytecode Compiler → Verifier → VM.

Optional compile-time path (off by default): AST → structured HIR → six passes, independently verified after each pass → Bytecode → Verifier. Integer MIR plans can be generated and independently verified with either compiler path.

Runtime: parameter feedback → guard every entry → integer MIR specialization; not ready, unsupported or mismatched → generic Bytecode. Arithmetic failure restores original-operation error semantics. HHY_SCALAR_REPLACEMENT=1 separately enables local List scalar replacement while retaining GC/quota reservations.

AST Interpreter remains the semantic oracle and explicit --engine ast fallback. HIR/MIR remain off by default; real workloads have not met admission gates.


{% hint style="info" %}
v1.7.0 defaults run, profile, and script shorthand to Bytecode. Use --engine ast or HHY_ENGINE=ast for immediate rollback. Engine and top-level labels identify the actual execution path.
{% endhint %}


## 20.8 Runtime resource limits

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


## 20.9 Stable exit codes

```text
0  success
1  unhandled runtime error
2  syntax or static-check error
3  invalid CLI usage
4  file I/O, process, or network error
5  timeout or cancellation
```


Automation should branch on stable exit codes rather than error text.
