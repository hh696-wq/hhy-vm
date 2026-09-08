# 26. HHY 语言状态报告 · 2026-09-07

发布 HHY 当前语义、Runtime、性能与工程质量状态，包含可复核的 CI 实测数据。

## 26.1 HHY 1.7.0 当前实现与兼容性

1.7.0 已通过本地测试、四平台 CI 和发行验收。语言规范仍为冻结的 1.0；官方 sample、HTML 以及 macOS/Linux Database 1.0.0 扩展已验证兼容。VS Code 0.2.0 与 Sublime 0.1.0 保持独立版本，语法和编辑器契约检查通过。


HIR、六个优化 pass、整数 MIR、类型反馈 guard/deopt 与局部 List 标量替换已交付。使用 HHY_COMPILER=ir、HHY_FEEDBACK_SPECIALIZATION=1、HHY_SCALAR_REPLACEMENT=1 显式启用；默认直接 Bytecode 不变。真实负载收益尚未满足默认启用门槛，标量替换保留原始 GC/配额预约。


[优化开关、验证与支持范围](/docs/COMPILER_IR.md)

v1.7.0 实施说明


[1.7.0 发行说明](/docs/RELEASE_NOTES_1.7.0.md)

已发布能力与边界


## 26.2 1.7.0 最新实测与发布证据

2026-09-08 本地 macOS arm64，HHY 1.7.0，默认编译配置，单次 profiler 原始输出如下。仅 4 个 CPU 样本，短任务热点比例和计时会波动，不用于判定性能提升。


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


| 平台 | 整数 MIR / 基线 | 局部 List / 基线 |
| --- | --- | --- |
| linux-arm64 | 0.7624× | 0.7495× |
| linux-x86_64 | 0.7067× | 0.6738× |
| macos-arm64 | 0.8392× | 0.8553× |
| windows-x86_64 | 0.8478× | 0.8326× |


{% hint style="info" %}
四平台数据来自发布提交 8fb0b8f 的 CI 34197497786，各 15 对交错样本。表中两个合成负载通过预算；core/json/closure 真实负载未达到 5% 收益门槛，优化仍默认关闭。标量替换保留原始 GC/配额预约。下方旧版本实测保留历史口径。
{% endhint %}


[查看 1.7.0 CI 与原始性能 artifacts](https://github.com/hh696-wq/hhy-vm/actions/runs/34197497786)

8fb0b8f · 2026-09-08


## 26.3 发布摘要

{% hint style="info" %}
截至 2026-09-08，HHY v1.7.0 已发布 Database 1.0.0 与资源作用域集成，并保留完整 Web Runtime：可嵌入 Runtime、HTTP/1.1、Router、Middleware、上传、流式响应、SSE、多 Worker 与可观测性。v1.4.0–v1.4.2 是能力里程碑，统一由 v1.4.3 正式发行。Bytecode 保持默认，AST 保留为语义 oracle 和显式回退。
{% endhint %}


| 报告维度 | 回答的问题 | 当前结论 |
| --- | --- | --- |
| 语言基线 | 核心语义是否稳定 | Pipe、Value、Stream、Error 与核心 callable contract 保持稳定，AST/Bytecode 持续对照 |
| Runtime 健康度 | 资源、内存与取消边界是否可靠 | 资源上限、GC stress、sanitizer、fuzz、故障注入与显式所有权治理通过 |
| 执行引擎 | Bytecode 是否适合默认启用 | 原生 Opcode 路径语义门禁与性能门禁通过，Bytecode 默认，AST 可显式回退 |
| 工程治理 | 变化是否可审计 | 四平台 CI、真实 workload、分层门禁、版本一致性和发行证据形成闭环 |


## 26.4 本期数据概览

| 数据项 | 结果 | 证据口径 |
| --- | --- | --- |
| 当前正式版本 | v1.7.0 | 四平台 Release、逐包 SHA-256 与 SHA256SUMS |
| 执行引擎 | Bytecode 默认 / AST 回退 | 完整双引擎套件与机器可读决策 |
| 核心 callable | 121 | Runtime Callable Contract Registry |
| 持续验证平台 | 4 个 | macOS arm64、Linux arm64、Linux x86_64、Windows x86_64 |
| v1.3.10 CI 引擎基线 | 历史测量通过 | 1M CPU 0.3695；短任务 1.0088；持续 JSON 1.0207 |
| v1.3.10 Profiler 开销 | 1.0269× / +2.786 ms | 历史 9 次样本；门槛 1.35× 且 12 ms |
| Bytecode 缓存 | 不准入 | 5 个负载 × 21 次；compile+verify 不构成主要成本 |
| 完整实战项目 | 6 个 | AST/Bytecode 端到端 acceptance 与稳定退出码 |


## 26.5 v1.5.0 / Database 1.0.0 发布验收

2026-09-07 发布。双数据库、双引擎、TLS、作用域清理、取消、CMS 安装数据库流程与跨平台发行门禁通过；本地百万行流式及每库 5 分钟持续测试报告可查。RDS 和 24 小时长稳仍待验证。


[查看 DB 1.0.0 验收记录](/docs/DATABASE_1.0.0_ACCEPTANCE.md)

HHY 1.5.0 · Database 1.0.0


[查看 HHY 1.5.0 发行说明](/docs/RELEASE_NOTES_1.5.0.md)

HHY 1.5.0 · Database 1.0.0


## 26.6 v1.4.3 Web Runtime 与发布验证

| 验证项 | 结果 | 测量范围 |
| --- | --- | --- |
| 可嵌入 Runtime | 100,000 次重复调用 | 单个已加载 Context；AST/Bytecode 输出差分检查 |
| HTTP 压力验证 | 1,000,000 次请求 · 0 失败 | 发布验证主机 loopback，16 个并发客户端 |
| HTTP 耗时与吞吐 | 131.439 秒 · 7,608.1 请求/秒 | 上述短请求负载的主机实测，不代表公网或业务服务吞吐 |
| HTTP 功能回归 | 端到端覆盖 | Router、Query、404/405/413/500、错误后恢复、Middleware、静态缓存、上传清理、CORS、signed cookie 与 gzip |
| 流式与运维 | 端到端覆盖 | Stream、SSE、Range、metrics、health、多 Worker 与开发热重载 |


{% hint style="info" %}
本节引用 v1.4.3 发行验证记录，并非 2026-09-04 重新压测。下方 CPU、Profiler、缓存与六语言比较保留原始 v1.3.10 版本和测量口径。
{% endhint %}


[查看 v1.4.3 发行验证记录](/docs/RELEASE_NOTES_1.4.3.md)

本站保存的发行说明，包含测试规模、结果与能力边界。


## 26.7 当前能力边界

| 范围 | 当前边界 |
| --- | --- |
| HTTP 服务 | Runtime 提供 HTTP/1.1；TLS 和 HTTP/2 由 Caddy、Nginx 或负载均衡器承担 |
| WebSocket | 不在 v1.4 能力范围内 |
| 执行引擎 | Bytecode 默认，保留 AST 回退；不提供 JIT、进程/磁盘 Bytecode 缓存或未验证外部 Bytecode 加载 |
| 原生集成 | C Embedding 使用 opaque 句柄和 JSON ABI；不等于开放 Runtime 内部结构或第三方 Native Extension ABI |


## 26.8 总体基线与兼容性

| 基线 | 稳定承诺 | 验证方式 |
| --- | --- | --- |
| 语言语义 | 不引入第二套 Pipe、Stream 或 Error 模型 | 规范示例、Parser/Checker fixtures 与合法程序回归 |
| Callable contract | 名称、arity、effect、lazy、cancellable 和 threading 可机器读取 | Contract Registry JSON 与当前 callable contract 一致性检查 |
| 诊断 | CLI 文本与 JSON/LSP 使用同一 Core 检查路径 | 诊断 schema 与 LSP 协议测试 |
| 扩展边界 | 第三方能力优先走 Process Extension Protocol | 清单完整性、Protocol 1 与官方扩展验收 |
| C Embedding | 公开 opaque Application/Context 句柄与 JSON ABI，不公开 Runtime 内部结构 | 单次加载、重复调用、请求隔离与 AST/Bytecode 差分验证 |


当前正式基线为 v1.7.0。Bytecode 是默认引擎，AST evaluator 继续作为语义 oracle，可通过 --engine ast 或 HHY_ENGINE=ast 显式使用。Compiler 产生的 Stream Kernel 必须独立通过 Verifier；动态或未知形状无损回退通用 Bytecode。


## 26.9 v1.2.2 发行与扩展状态

| 能力 | 当前状态 | 验收结果 |
| --- | --- | --- |
| 扩展分发 | Ed25519 签名 Registry、确定性依赖解析 | 篡改、来源不明和依赖冲突稳定拒绝 |
| 可复现环境 | Lockfile、content-addressed 离线缓存 | 同一 lock 得到同一依赖图，干净环境可离线重建 |
| 安全变更 | 事务式安装、升级和显式回滚 | 失败升级不破坏旧环境 |
| HTML 0.2.0 | Lexbor、CSS selector、单次解析多字段投影 | 畸形 HTML、硬上限、截断和结构化错误通过四平台验收 |
| 协议决策 | 保留同步有界批量 API | HTML 保持同步批量；DB 1.0 已使用作用域句柄、取消和宿主游标 Stream；通用 Stream credit 未开放 |


{% hint style="info" %}
v1.2.2 正式 Release 已包含 macOS arm64、Linux x86_64、Linux arm64、Windows x86_64 归档、逐包 SHA-256 与合并 SHA256SUMS。HTML 扩展保持 effect = none，不自行读取文件、访问网络或执行子进程。
{% endhint %}


[查看 HHY Language v1.2.2 正式发行](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.2.2)

下载四平台归档、校验文件并查看完整发行说明。


## 26.10 v1.3.7–v1.3.10 Bytecode 加固状态

| 版本 | 核心交付 | 已验证结论 |
| --- | --- | --- |
| v1.3.7 | 具名 specialization metadata、统一 stack/error、fallback reason | 无 magic kind；三路径差分与变形测试通过 |
| v1.3.8 | Compiler 生成版本化 Stream Kernel IR | Runtime 不读取 AST 形状；Kernel 独立 verify；动态形状安全回退 |
| v1.3.9 | 普通执行与 Profiler 共用优化决策 | kernel/opcode、取消、CPU/Heap 归因与机器报告一致；开销门禁通过 |
| v1.3.10 | 真实性能触发的缓存治理 | 数据未触发准入；无进程/磁盘缓存；不接受未验证外部 Bytecode |


{% hint style="info" %}
四个版本均按顺序完成实现、Release/Debug 测试、sanitizer、fuzz、真实性能、四平台 Actions、正式 Release 和 Homebrew Formula 校验后才进入下一版本。
{% endhint %}


[查看 HHY Language v1.7.0 正式发行](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.7.0)

包含四平台归档、逐包 SHA-256、SHA256SUMS 与 Web Runtime 发行说明。


## 26.11 历史性能基线 · v1.3.10

最终 v1.3.10 CI 数据来自提交 4ddc8c3、GitHub Actions Ubuntu 24.04 的 schema-2 paired/interleaved benchmark 和独立 Profiler/缓存决策 artifact。数值是 Bytecode/AST 墙钟比；小于 1 表示 Bytecode 更快。


| 门禁 | 实测 | 上限 | 结果 |
| --- | --- | --- | --- |
| 1M CPU | 0.3695× | 0.90× | 通过 |
| 短任务 | 1.0088× | 1.25× | 通过 |
| 持续 JSON/I/O | 1.0207× | 1.10× | 通过 |
| Profiler 开销 | 1.0269× / +2.786 ms | 1.35× / +12 ms | 通过 |


| 缓存准入负载 | compile+verify 中位数 | 冷运行中位数 | 占比 |
| --- | --- | --- | --- |
| Hello | 0.0097 ms | 5.860 ms | 0.1649% |
| Advanced Flow | 0.0269 ms | 5.916 ms | 0.4546% |
| Stdlib | 0.0286 ms | 7.003 ms | 0.4086% |
| Sustained JSON | 0.0089 ms | 33.518 ms | 0.0266% |
| Core Flow 1M | 0.0105 ms | 166.285 ms | 0.0063% |


{% hint style="info" %}
缓存联合门槛为 compile+verify ≥ 1 ms 且占冷运行 ≥ 20%。五项均远低于门槛，即使假设缓存读取成本为零也无可复现收益，因此 v1.3.10 不实现进程/磁盘缓存，并继续拒绝未验证外部 Bytecode。
{% endhint %}


## 26.12 v1.3.10 六语言同机重测

2026-09-01 在 macOS 26.6.2 arm64 上重新实测 HHY 1.3.10、PHP 8.5.10、Go 1.27.0、Python 3.14.7、Lua 5.5.1 和 OpenJDK 26.0.2.1。固定任务为 range(0, 1,000,000) → 乘 2 → 保留可被 3 整除的值 → 稳定去重 → 物化 → 计数；六种实现都验证输出 333334。每种语言先预热 2 次，再做两轮独立测量；每轮 7 个 fresh process，固定种子随机交错运行，墙钟包含进程启动；Go 与 Java 预先编译，编译时间不计入。


| 实现 | 版本 | 第 1 轮中位数 | 第 2 轮中位数 | 两轮范围 |
| --- | --- | --- | --- | --- |
| Go | 1.27.0 | 7.995 ms | 7.969 ms | 7.622–15.325 ms |
| Lua | 5.5.1 | 18.368 ms | 17.957 ms | 17.582–19.822 ms |
| PHP | 8.5.10 | 43.174 ms | 43.549 ms | 42.588–49.033 ms |
| Java | OpenJDK 26.0.2.1 | 49.394 ms | 48.153 ms | 46.995–53.066 ms |
| HHY Bytecode | 1.3.10 | 55.297 ms | 53.404 ms | 51.027–78.702 ms |
| Python | 3.14.7 | 81.747 ms | 86.459 ms | 79.993–87.774 ms |


| 比较 | 第 1 轮 | 第 2 轮 | 解读 |
| --- | --- | --- | --- |
| HHY / PHP | 1.28× | 1.23× | 该任务 HHY 比 PHP 多用约 23%–28% 墙钟 |
| HHY / Java | 1.12× | 1.11× | 包含 JVM fresh-process 启动时，Java 略快 |
| HHY / Python | 0.68× | 0.62× | HHY 在该任务上用时更少 |
| HHY / Lua | 3.01× | 2.97× | Lua 在该整数循环上更快 |
| HHY / Go | 6.92× | 6.70× | 预编译 Go 显著更快 |


{% hint style="info" %}
这是一个特定 CPU/物化 workload，不是通用语言排名。各实现使用惯用循环与去重容器；Java 使用 HashSet/ArrayList 并包含 fresh JVM 启动。全部 84 个计时样本、版本、顺序和源程序保存在本地 performance-analysis/2026-09-01-v1.3.10-language-comparison/，该目录按项目规则不提交 GitHub。
{% endhint %}


## 26.13 治理结论与后续观察

- 总体状态：v1.7.0 / DB 1.0.0 已通过本地、CI 与跨平台发行验收；RDS 实机和 24 小时长稳仍待补验。
- Web 结论：可嵌入 Runtime、HTTP、Router、Middleware、上传、Stream/SSE、多 Worker 与可观测性一次性交付。
- 执行引擎策略：Bytecode 默认；AST 永久保留为语义 oracle、差分测试和 --engine ast 紧急回退。
- 性能结论：Web 百万短请求 0 失败；既有 CPU、短任务、持续 JSON 与 Profiler 门禁继续通过。
- 缓存结论：compile+verify 不构成冷运行主要成本；当前不引入缓存，未来必须由新数据重新触发评审。
- 更新规则：发布基线、测量方法、引擎/缓存决策或总体风险结论变化时同步更新本报告。


[查看 v1.3.11 持续验证证据](https://github.com/hh696-wq/hhy-vm/actions/runs/33581452558)

四平台构建、sanitizer、fuzz、性能门禁与真实项目验收。
