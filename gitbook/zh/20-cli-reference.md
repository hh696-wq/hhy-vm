# 20. CLI 参考

运行、检查、格式化、REPL、dry-run 与性能分析。

## 20.1 HHY 1.7.0 当前实现与兼容性

1.7.0 已通过本地测试、四平台 CI 和发行验收。语言规范仍为冻结的 1.0；官方 sample、HTML 以及 macOS/Linux Database 1.0.0 扩展已验证兼容。VS Code 0.2.0 与 Sublime 0.1.0 保持独立版本，语法和编辑器契约检查通过。


HIR、六个优化 pass、整数 MIR、类型反馈 guard/deopt 与局部 List 标量替换已交付。使用 HHY_COMPILER=ir、HHY_FEEDBACK_SPECIALIZATION=1、HHY_SCALAR_REPLACEMENT=1 显式启用；默认直接 Bytecode 不变。真实负载收益尚未满足默认启用门槛，标量替换保留原始 GC/配额预约。


[优化开关、验证与支持范围](/docs/COMPILER_IR.md)

v1.7.0 实施说明


[1.7.0 发行说明](/docs/RELEASE_NOTES_1.7.0.md)

已发布能力与边界


## 20.2 版本与发布信息

使用 --version 确认当前二进制版本、项目作者、开源许可证和官方联系方式。源码构建使用 ./build/hhy；发行包或安装到 PATH 后可直接使用 hhy。


### HHY · 版本信息

```console
$ ./build/hhy --version
hhy 1.7.0
© 2026 HHY Language contributors
Author: houhuiyang
License: Apache License 2.0
https://hhylang.dev/
huiyang.hou@qq.com
```

HHY 1.7.0 的真实命令输出；版本、作者、许可证、官网和联系邮箱由 CLI 直接提供。


{% hint style="info" %}
如果从正式发行包运行，请在解压目录执行 ./bin/hhy --version；如果已经 make install 或加入 PATH，则执行 hhy --version。
{% endhint %}


## 20.3 完整命令

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


| 命令 | 用途 |
| --- | --- |
| hhy run | 使用默认 Bytecode 引擎运行脚本并传递 args |
| hhy run --engine ast\|bytecode | 显式选择 AST 回退或 Bytecode 引擎 |
| hhy serve | 运行常驻 Web 应用；应用通过 web.listen 配置地址、端口和 Worker |
| hhy serve --dev | 监听源码变化并自动重新加载 Web 应用 |
| hhy serve --engine ast\|bytecode | 选择 Web handler 使用的执行引擎 |
| hhy serve --limit NAME=VALUE | 为 Web Runtime 设置可重复的资源限制 |
| hhy profile | 分析实际引擎的 CPU 热点、调用次数和托管 Heap 分配 |
| hhy repl | 启动交互环境 |
| hhy check | 检查语法和核心语义 |
| hhy fmt | 写入官方格式 |
| hhy fmt --check | 只检查格式 |
| hhy ast | 输出 AST |
| hhy bytecode | 编译、验证并反汇编 Bytecode |
| hhy bytecode --metrics | 输出 compile/verify/prepare 的缓存准入测量 JSON |
| hhy tokens | 输出 Lexer Token |
| hhy run --dry-run | 预览脱敏执行计划 |


hhy script.hhy 是 hhy run script.hhy 的简写。v1.7.0 默认使用 Bytecode；可用 --engine ast 或 HHY_ENGINE=ast 立即回退。脚本参数可能以 - 开头时，在 Runtime 选项后使用 -- 分隔。


## 20.4 serve · 常驻 Web 应用

serve 加载并检查一个 HHY Web 应用，然后保持进程运行。--dev 在源码变化后安全重载；--engine 选择 AST 或 Bytecode；--limit 可重复设置 Runtime 资源上限。host、port、max_body、workers 等 HTTP 参数由应用中的 web.listen 显式配置，额外位置参数通过只读 args 传给应用。


```sh
hhy serve app.hhy
hhy serve --dev app.hhy -- 8080
hhy serve --engine bytecode \
  --limit max_memory=256mib \
  --limit max_runtime=30s \
  app.hhy -- 8080 production
```


| 选项/参数 | 行为 |
| --- | --- |
| --dev | 监测入口及其模块源码，变化后重新加载应用 |
| --engine ast\|bytecode | 显式选择 handler 执行引擎；默认 Bytecode |
| --limit NAME=VALUE | 覆盖内存、运行时间、文件、进程、并行度、HTTP Body、正则或递归限制，可重复 |
| --dry-run | 执行计划但拦截外部副作用；主要用于启动配置检查 |
| -- | 结束 Runtime 选项，后续值原样进入应用 args |


{% hint style="info" %}
serve 不会替应用猜测端口；入口文件必须最终调用 web.listen。生产环境应在 Caddy、Nginx 或云负载均衡器之后运行。
{% endhint %}


## 20.5 Bytecode 缓存准入证据

v1.7.0 新增只读的 --metrics 输出，用真实 compile+verify 与 execution-plan verify 时间评估缓存是否值得引入。固定五负载、21 次配对冷进程测量中，compile+verify 中位数为 0.004–0.012 ms，仅占冷运行墙钟 0.0078%–0.1341%，没有达到 1 ms 且 20% 的双门槛。


```console
$ hhy bytecode --metrics examples/00-hello.hhy
{"bytecode_format_version":1,"compile_verify_ns":9000,"constants":21,"instructions":40,"schema_version":1,"source_bytes":167,"stream_kernel_version":1,"stream_kernels":1,"tool":"hhy bytecode --metrics","verify_prepare_ns":4000}
```


{% hint style="info" %}
因此当前没有进程内或磁盘 Bytecode 缓存，也不接受第三方预编译 Bytecode。未来只有真实性能证据触发评审后，才可实现绑定源码、递归依赖、语言/格式/Kernel 版本、编译 feature、target 与安全策略的完整指纹；命中后仍必须经过 checksum、有界解析、完整 Verifier 和 execution-plan verify。
{% endhint %}


## 20.6 CPU 与 Heap 性能分析

profile 会真实执行脚本，默认在一次运行中同时收集 CPU 和托管 Heap 数据。报告写入 stderr，因此脚本 stdout 保持不变；命令返回脚本原有退出码。


```sh
hhy profile --engine bytecode examples/09-profile-algorithms.hhy -- fibonacci 20
hhy profile --engine ast examples/09-profile-algorithms.hhy -- fibonacci 20
hhy profile --heap --format json --output profile.json examples/09-profile-algorithms.hhy -- fibonacci 20
```


| 选项 | 行为 |
| --- | --- |
| --engine ast\|bytecode | 显式分析 AST 或 Bytecode；默认 bytecode，报告 Summary 显示实际 Engine |
| --cpu | 只收集 1ms 进程 CPU 采样和调用次数 |
| --heap | 只收集累计分配、分配次数、Heap 峰值和 GC 后占用 |
| --format text\|json | 选择人类可读或机器可读报告；默认 text |
| --output <path> | 把报告写入文件，而不是 stderr |
| --limit NAME=VALUE | 与 run 相同，覆盖 Runtime 资源限制 |
| --dry-run | 与 run 相同，拦截外部副作用并分析计划执行 |


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
HHY 1.7.0 · 2026-09-08 · macOS arm64 · 本地单次实测；CPU 样本少于 10 个，计时和热点占比会波动。使用默认编译配置，未启用可选优化。
{% endhint %}


{% hint style="info" %}
CPU 使用进程 CPU 时间采样，文件、HTTP 和进程等待不会被误算成 CPU 热点。运行不足数毫秒的脚本可能样本太少，应增大输入或重复负载。Heap 只统计 HHY 的 Boehm GC 托管内存，不包含扩展子进程或原生库自行管理的内存。
{% endhint %}


## 20.7 解释器性能架构演进

v1.7.0 默认使用经 Compiler/Verifier 验证的 Bytecode VM。原生 Opcode dispatch、静态槽位、可复用调用帧和低分配 Closure 快路径降低 CPU 开销；AST Interpreter 永久保留为语义 oracle 与显式回退。


默认：Source → AST → Bytecode Compiler → Verifier → VM。

可选编译期路径（默认关闭）：AST → 结构化 HIR → 六个 pass（每个 pass 后独立验证）→ Bytecode → Verifier。整数 MIR 计划可随任一编译路径生成并独立验证。

运行时：参数类型反馈 → 每次入口 guard → 整数 MIR 特化；未稳定、不支持或失配 → 通用 Bytecode。算术失败恢复原操作的错误语义。HHY_SCALAR_REPLACEMENT=1 另行启用局部 List 标量替换，保留 GC/配额预约。

AST Interpreter 永久作为语义 oracle 和 --engine ast 显式回退。HIR/MIR 默认关闭，真实负载收益尚未满足默认准入门槛。


{% hint style="info" %}
v1.7.0 的 run、profile 与脚本简写默认使用 Bytecode；可用 --engine ast 或 HHY_ENGINE=ast 立即回退。profile 的 Engine、<top-level>/<bytecode-top-level> 会明确标记实际路径。
{% endhint %}


## 20.8 Runtime 资源限制

run 的 --limit NAME=VALUE 可以重复出现。大小必须带 b/kb/mb/gb/kib/mib/gib，时间必须带 ns/us/ms/s/min/h，计数值不带单位。


```sh
hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
```


| 限制 | 默认值 |
| --- | --- |
| max_memory | 512mib |
| max_open_files | 256 |
| max_processes | 16 |
| max_parallelism | 16 |
| max_http_body | 16mib |
| max_regex_steps | 1000000 |
| max_recursion | 256 |
| max_runtime | 0（CLI 默认不设总时限） |


## 20.9 稳定退出码

```text
0  成功
1  未处理的运行时错误
2  语法或静态检查错误
3  CLI 用法错误
4  文件 I/O、进程或网络错误
5  超时或取消
```


自动化脚本应按稳定退出码而不是错误文本进行分支。
