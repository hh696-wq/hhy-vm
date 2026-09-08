# 24. 语言与 VM 演进路线图

v1.7.0 交付 Database 1.0.0、连接池、远程 TLS 与 Runtime 资源集成，保留 Web Runtime 和 AST/Bytecode 双引擎。

## 24.1 HHY 1.7.0 当前实现与兼容性

1.7.0 已通过本地测试、四平台 CI 和发行验收。语言规范仍为冻结的 1.0；官方 sample、HTML 以及 macOS/Linux Database 1.0.0 扩展已验证兼容。VS Code 0.2.0 与 Sublime 0.1.0 保持独立版本，语法和编辑器契约检查通过。


HIR、六个优化 pass、整数 MIR、类型反馈 guard/deopt 与局部 List 标量替换已交付。使用 HHY_COMPILER=ir、HHY_FEEDBACK_SPECIALIZATION=1、HHY_SCALAR_REPLACEMENT=1 显式启用；默认直接 Bytecode 不变。真实负载收益尚未满足默认启用门槛，标量替换保留原始 GC/配额预约。


[优化开关、验证与支持范围](/docs/COMPILER_IR.md)

v1.7.0 实施说明


[1.7.0 发行说明](/docs/RELEASE_NOTES_1.7.0.md)

已发布能力与边界


## 24.2 语言 / VM 演进路线图

当前 v1.7.0 · Database 1.0.0 已发布，延续完整 Web Runtime


| 版本 | 发布日期 | 主要变化 | 验收状态 |
| --- | --- | --- | --- |
| v1.0.0 · 已发布 | 2026-08-25 | 核心语言与 VM 语义冻结 | Pipe、Value、Stream、Error、核心标准库和三平台发行证据完成 |
| v1.1.0 · 已发布 | 2026-08-26 | 本地进程扩展与官方数据库扩展 | 安装/加载完整性、Protocol 1 同步调用、database 0.2.0 和三平台发行证据完成 |
| v1.1.1 · 已发布 | 2026-08-27 | 性能优化与临界资源稳定性 | hhy profile、解释器热点基线和 Runtime 资源边界完成 |
| v1.1.2 · 已发布 | 2026-08-27 | HTML 扩展与静态采集框架 | 三平台 CI、扩展协议测试、本机 fixture 与真实 hhylang.dev 抓取完成 |
| v1.1.3 · 已发布 | 2026-08-28 | Runtime 正确性与性能加固 | GC 压力回归、sanitizer、哈希索引、稳定诊断与三平台发布证据完成 |
| v1.1.4 · 已发布 | 2026-08-28 | 安全静态 Spider | URL 规范化、链接发现、Frontier、边界、指纹去重与连接级 SSRF 防护 |
| v1.1.5 · 已发布 | 2026-08-30 | 可恢复 Spider 与浏览器渲染 | 持久 Frontier、断点恢复、流式落盘、可选 Playwright 与 Windows MSYS2 构建证据 |
| v1.1.6 · 已完成 | 2026-08-31 | 稳定基线与测试治理 | 宿主能力探测、分层 CI、机器可读性能基线与发布一致性门禁 |
| v1.1.7 · 已完成 | 2026-08-31 | 诊断与编辑器基线 | 版本化 JSON diagnostics、Contract Registry JSON、最小 LSP 与 VS Code 编辑闭环 |
| v1.1.8 · 已完成 | 2026-08-31 | Runtime 渐进治理 | 首个模块边界、内部所有权 API、sanitizer/GC stress 与阻断式性能回归门禁 |
| v1.2.0 · 已发布 | 2026-08-31 | 官方扩展分发与签名 | 命名空间身份、Ed25519 签名索引与包描述、确定性依赖解析、dry-run 和事务式安装 |
| v1.2.1 · 已发布 | 2026-09-01 | 锁定、离线与安全回滚 | 同一 lock 得到同一依赖图；离线可重建；失败升级不破坏旧环境 |
| v1.2.2 · 已发布 | 2026-09-01 | 官方 HTML 复杂扩展验证 | 真实 fixture、可观察截断、结构化错误和四平台发行全部通过 |
| v1.3.0-alpha · 已预发布 | 2026-09-01 | Bytecode 编译器骨架 | 核心语法可编译；非法 Bytecode 可拒绝；AST 仍为默认引擎 |
| v1.3.0-beta · 阶段门禁已完成 | 并入 v1.3.0 | Bytecode VM 执行核心 | 执行桥、Verifier、资源边界和完整双引擎 fixture 已验收 |
| v1.3.0-rc · 阶段门禁已完成 | 并入 v1.3.0 | 性能、Profiler、Stack trace 与默认切换门禁 | 三平台和故障证据已完整；CPU 收益门槛未通过，因此 AST 保持默认 |
| v1.3.0 · 已发布 | 2026-09-01 | 可选 Bytecode 正式执行路径 | 完整套件双引擎通过；性能决策保持 AST 默认并保留显式回退 |
| v1.3.1 · 已发布 | 2026-09-01 | 真实负载兼容加固 | 官方 workload 双引擎矩阵和能力探测证据已通过三平台 CI |
| v1.3.2 · 已发布 | 2026-09-01 | VM 内部边界稳定化 | 版本化 Bytecode Runtime 边界、静态治理与持续 AST oracle |
| v1.3.3 · 已完成 | 并入 v1.3.5 | 原生 Opcode 执行闭环 | 正常 Bytecode 路径不再调用 AST evaluator，完整双引擎语义对照通过 |
| v1.3.4 · 已完成 | 并入 v1.3.5 | VM 数据路径优化 | 1M CPU workload ratio 0.6805；短任务与 JSON/I/O 无明显回退 |
| v1.3.5 · 已发布 | 2026-09-01 | Bytecode 默认引擎 | run/profile/简写默认 Bytecode；AST 显式回退与 oracle 永久保留 |
| v1.3.6 · 已发布 | 2026-09-01 | Stream Int fusion 性能闭环 | 安全形状保守融合；未知形状无损回退；双引擎与跨平台门禁通过 |
| v1.3.7 · 已发布 | 2026-09-01 | 特化实现加固 | 具名 metadata、统一 stack/error、fallback reason 与三路径差分通过 |
| v1.3.8 · 已发布 | 2026-09-01 | Compiler/Verifier 优化 IR | Runtime 不读取 AST 形状；Stream Kernel 独立验证并可靠回退 |
| v1.3.9 · 已发布 | 2026-09-01 | Profiler 与资源一致性 | 普通执行与观测共用优化决策；开销、取消和 Heap 归因门禁通过 |
| v1.3.10 · 已发布 | 2026-09-01 | Bytecode 缓存治理 | 五负载实测未达准入门槛；不实现缓存，不接受未验证外部 Bytecode |
| v1.3.11 · 已发布 | 2026-09-02 | Runtime 错误路径加固 | 结构化错误完整构造后发布；原始 fuzz 输入纳入永久回归 |
| v1.4.0 · 已完成 | 2026-09-03 | Web Runtime 基础 | Embedding、应用一次加载、handler 重复调用与请求隔离门禁完成 |
| v1.4.1 · 已完成 | 2026-09-03 | Web MVP | HTTP/1.1、Router、Query/Header/Cookie、JSON API 与稳定错误完成 |
| v1.4.2 · 已完成 | 2026-09-03 | Web 应用能力 | Middleware、静态文件、上传、CORS、signed cookie、gzip 与热重载完成 |
| v1.4.3 · 已发布 | 2026-09-03 | 流式与多 Worker | SSE、Stream、大文件 Range、多进程和可观测性完成；百万请求 0 失败 |
| v1.5.0 · 已发布 | 2026-09-07 | Database 1.0.0 与 Runtime 资源集成 | 连接池、远程 TLS、事务、游标、精确类型、取消与 Worker 隔离；双数据库、双引擎和发行 Actions 通过；RDS 实机待验收 |
| v1.6.0 · 评估交付 | 2026-09-08 | 指令选择与 dispatch 画像 | 版本化 opcode/pair/triple 统计与开关交付；superinstruction 未达准入门槛；随 v1.7.0 发布 |
| v1.6.1 · 实现交付 | 2026-09-08 | 调用、Closure 与异常展开 | 紧凑调用布局、闭包路径与异常区域表实验通过差分验证；显式启用，随 v1.7.0 发布 |
| v1.6.2 · 评估交付 | 2026-09-08 | 条件式 inline cache | Map slot 实验与稳定/变化调用点评估完成；未达默认准入门槛，保留通用回退；随 v1.7.0 发布 |
| v1.6.3 · 评估交付 | 2026-09-08 | GC、内存与调度决策 | 分配/暂停及调度需求评估完成；未准入新的 GC 或 async scheduler；决策随 v1.7.0 交付 |
| v1.7.0 · 当前 | 2026-09-08 | Optimizing Compiler & Runtime | 四平台 CI 与发行验收通过；HIR、六个 pass、整数 MIR、guard/deopt、局部 List 标量替换交付；优化默认关闭 |


## 24.3 后续规划

| 版本 | 建议窗口 | 计划交付 | 验收条件 |
| --- | --- | --- | --- |
| v2.0 · 条件规划 | 生态证据充分后 | 生态开放与 ABI 决策 | 至少两个真实集成证明进程协议不足；否则继续使用进程协议并不开放 Native ABI |


说明：以上时间为建议窗口，不构成发布承诺。


## 24.4 演进原则

| 原则 | 约束 |
| --- | --- |
| 语义先冻结 | Pipe、Value、Stream、Error 与取消语义先稳定，再扩展生态表面 |
| 可用、可测先于高性能 | 每项能力先具备确定错误、资源上限和跨平台测试，再进行优化 |
| 协议优先 | 第三方能力优先通过 Process Extension Protocol 接入，不并行发明第二套语言语义 |
| ABI 有条件开放 | Native ABI 只有在 Runtime 足够稳定且测量证明必要时才评估；不开放也是有效结论 |


路线图每个季度应重新评审一次：只调整尚未冻结版本；已经发布的语义、协议兼容承诺和迁移路径不能因排期变化而被削弱。


## 24.5 明确不在路线中承诺的事项

- 不会为了版本号引入第二套 Pipe、Stream 或 Error 模型。
- 不会在缺少兼容策略时直接公开 Runtime 内部 C 结构体。
- 不会把建议时间窗口当作牺牲测试、安全或跨平台验证的理由。
- 不会同时推进远程包仓库、Native ABI 和多套官方扩展而绕过阶段验收。
