# HHY 语言状态报告

报告日期：2026-09-01<br>
报告对象：[HHY 语言状态报告网页](http://127.0.0.1:9800/zh/learn/language-health-report)<br>
当前正式版本：HHY v1.3.2<br>
发布提交：`92e5b59`（v1.3.2）<br>
报告结论：**健康，已发布；AST/Bytecode 语义与跨平台门禁完整，AST 依据真实性能证据继续作为默认引擎。**

## 1. 执行摘要

截至 2026-09-01，HHY 已正式发布 v1.3.0、v1.3.1 和 v1.3.2。核心语言语义保持稳定；可选 Bytecode 引擎、Verifier、Profiler、HHY Stack trace、故障注入、真实 workload 双引擎矩阵和版本化 `bytecode_runtime` 内部边界均已交付。

三个 v1.3 稳定 Release 均包含四个平台归档、逐包 SHA-256 和合并 `SHA256SUMS`。双引擎语义门禁通过，但 CPU-bound benchmark 没有达到默认切换门槛，因此 AST 保持默认和语义 oracle，Bytecode 通过 `--engine bytecode` 显式启用。

网页结构、信息分层和历史性能口径继续有效；本次已把当前版本、执行引擎、跨平台证据、风险结论和路线图阶段同步到 v1.3.2。v1.1.8 性能数据只作为明确标注的历史基线保留。

## 2. 健康度总览

| 维度 | 状态 | 结论 | 主要证据 |
| --- | --- | --- | --- |
| 语言语义 | 健康 | Pipe、Value、Stream、Error 与核心 callable contract 保持稳定 | 规范示例、Parser/Checker fixtures、96 项 callable contract 检查 |
| Runtime | 健康 | 资源上限、所有权、GC stress、sanitizer、fuzz 和稳定错误形成门禁 | Release/Debug suite、Runtime governance、CI 分层门禁 |
| 执行引擎 | 健康 | AST 默认，Bytecode 可选；完整双引擎语义对照持续通过 | Verifier、Profiler、Stack trace、故障注入和 AST oracle |
| 扩展协议 | 健康 | Protocol 1 保持兼容，只追加可忽略的错误元数据 | `operation`、`stage`、`cause` 兼容测试；旧扩展无需重发 |
| 扩展分发 | 健康 | 签名、依赖、锁定、离线、可复现安装和回滚形成闭环 | v1.2.0–v1.2.2 发行与 Registry 验收 |
| 复杂扩展 | 健康 | HTML 0.2.0 可安全解析畸形 HTML 并有界批量抽取 | Lexbor、CSS selector、真实目录 fixture、硬上限与结构化错误 |
| 跨平台发行 | 健康 | 四个平台构建与正式资产完整 | macOS arm64、Linux x86_64、Linux arm64、Windows x86_64 |
| 性能治理 | 健康 | 固定 workload、五次采样中位数和阻断预算持续有效 | `benchmarks/performance-budget.json` 与 CI baseline artifact |
| 网站内容时效性 | 健康 | 学习手册已同步 v1.3.2、正式发行证据和默认引擎决策 | 生产构建与全局过期版本文案审计 |

## 3. 当前发布基线

### 3.1 v1.2.2 已交付能力

- 官方 `html` 0.2.0 使用 Lexbor 解析不可信或畸形 HTML；
- 支持有界 CSS 选择和单次解析、多字段结构化抽取；
- `html.extract_report` 返回 `rows`、`matched`、`returned`、`truncated` 和 `input_bytes`；
- 扩展保持 `effect = none`，不自行读取文件、访问网络或执行子进程；
- HHY Error 可保留扩展 `operation`、`stage` 与 `cause`，同时兼容旧 Protocol 1 扩展；
- 已验证签名 Registry 安装、确定性 lock、离线重建、篡改拒绝、升级和回滚；
- Windows 扩展原生 DLL 放入包内私有 `lib`，Runtime 启动扩展时使用该受控路径，避免依赖宿主机全局 PATH。

### 3.2 协议决策

当前实测边界为 768 KiB 输入、最多 10,000 条结果和 1 MiB 协议消息。真实 HTML 集成可以通过同步、有界、批量 API 完成任务，没有证据支持在 v1.2.2 引入 Stream credit、跨调用取消或 Opaque Handle。这些能力继续作为条件规划项，只有真实集成和兼容测试证明必要时才进入协议。

## 4. 可复核发行证据

| 证据 | 结果 |
| --- | --- |
| 开发门禁 | [Release Evidence #33463216421](https://github.com/hh696-wq/hhy-vm/actions/runs/33463216421) 全部通过 |
| 标签后门禁 | [Release Evidence #33463496696](https://github.com/hh696-wq/hhy-vm/actions/runs/33463496696) 全部通过 |
| 网站检查 | [Website #33463496691](https://github.com/hh696-wq/hhy-vm/actions/runs/33463496691) 通过 |
| 正式发行 | [Publish HHY release #33463496715](https://github.com/hh696-wq/hhy-vm/actions/runs/33463496715) 通过 |
| GitHub Release | [HHY Language v1.2.2](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.2.2)，非 draft、非 prerelease |

### 4.1 v1.3 稳定版本证据

| 证据 | 结果 |
| --- | --- |
| 最终主分支门禁 | [Release Evidence #33470597434](https://github.com/hh696-wq/hhy-vm/actions/runs/33470597434) 全部通过 |
| 最终网站检查 | [Website #33470597435](https://github.com/hh696-wq/hhy-vm/actions/runs/33470597435) 通过 |
| v1.3.0 Release | [正式发行](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.3.0) |
| v1.3.1 Release | [正式发行](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.3.1) |
| v1.3.2 Release | [正式发行（latest）](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.3.2) |

正式 Release 资产：

- `hhy-1.2.2-darwin-arm64.tar.gz`；
- `hhy-1.2.2-linux-arm64.tar.gz`；
- `hhy-1.2.2-linux-x86_64.tar.gz`；
- `hhy-1.2.2-windows-x86_64.tar.gz`；
- 每个平台对应的 `.sha256`；
- 合并校验文件 `SHA256SUMS`。

### 4.2 v1.3.2 Bytecode VM 状态

- `hhy run` 与 `hhy profile` 支持 `--engine ast|bytecode`；
- AST/Bytecode 完整 fixtures 和真实 workload 持续对照；
- 非法 Bytecode 由 Verifier 拒绝，故障、取消和资源路径纳入门禁；
- 版本化 compiler/VM 边界阻止 Runtime 绕过 Compiler/Verifier；
- 最新决策中 CPU ratio 为 `1.0121`，高于默认切换上限 `0.90`；短任务和 JSON/I/O 回退门禁通过；
- 结论：Bytecode 已正式可用，但 AST 继续默认。

## 5. 性能状态

性能数据用于 HHY 自身回归治理，不用于跨语言性能排名。

### 5.1 历史本机 v1.2.2 样本

环境：macOS 26.6.2 arm64，10 CPU，Python 3.14.7；提交 `6c850b2`。每项连续运行 5 次，报告中位数。

| Workload | 中位数 | 最小–最大 | 绝对预算 | 预算占用 |
| --- | ---: | ---: | ---: | ---: |
| CLI `--version` | 3.478 ms | 3.247–3.891 ms | 100 ms | 3.5% |
| Basic Flow | 7.855 ms | 7.761–9.313 ms | 250 ms | 3.1% |
| Core Flow 100k | 18.729 ms | 18.608–19.048 ms | 500 ms | 3.7% |
| JSON Flow | 7.943 ms | 7.626–8.531 ms | 250 ms | 3.2% |

四项 workload 均远低于绝对预算。不同机器的绝对耗时不可直接横向比较；正式趋势判断应继续使用 CI 中相同环境、相同输入和相同采样方法产生的 artifact。

### 5.2 网页现有历史样本

目标网页展示的是 v1.1.8 / 提交 `43db191` 的 Ubuntu 24.04 x86_64 样本。该数据仍可作为历史测量点保留，但必须明确标注为“历史基线”，不应继续描述为“当前语言基线”。

## 6. 学习手册一致性检查

### 6.1 保持正确的内容

- 页面按发布摘要、数据概览、兼容性、性能和治理结论组织，结构清楚；
- 96 项核心 callable、21 个完整规范代码块、4 个持续验证平台等口径有自动检查支撑；
- 性能数据明确声明只用于自身回归，不做跨语言排名；
- Process Extension Protocol 优先、Native ABI 证据驱动的治理原则仍然正确。

### 6.2 本次已更新的内容

| 页面字段 | 当前展示 | 应更新为 |
| --- | --- | --- |
| 当前语言基线 | v1.2.2 | v1.3.2 |
| 执行引擎 | Bytecode 仍是未来方向 | Bytecode 已可选，AST 依据性能证据保持默认 |
| v1.3 beta/rc | 仍显示开发中 | 标记为已完成并合入 v1.3.0 的阶段门禁 |
| 发行证据 | v1.2.2 Actions | 增加 v1.3.0–v1.3.2 Releases 和最终四平台门禁 |
| 性能表述 | 旧数据容易被理解为当前版本 | 明确 v1.1.8/v1.2.2 为历史样本，当前引擎决策使用独立 benchmark |

## 7. 风险与后续观察

1. **Runtime 集中度**：仍需按稳定职责渐进拆分，不能以大规模重写替代可验证治理。
2. **文档状态漂移**：版本发布后网页报告未同步更新，会削弱“当前状态报告”的可信度；应把版本、日期和 Release 证据纳入一致性检查。
3. **Windows 原生依赖**：HTML 扩展已修复私有 DLL 打包，但后续新增原生扩展必须复用相同发行验收。
4. **协议扩张压力**：继续坚持真实负载触发；没有证据时不增加 Stream credit、跨调用取消或 Handle 生命周期复杂度。
5. **性能可比性**：本机与 CI 样本必须分别标注环境；趋势报告以同环境 CI artifact 为准。

## 8. 总体结论

HHY v1.3.2 的语言语义、AST/Bytecode 一致性、Runtime、扩展协议、扩展分发和四平台发行状态为健康。Bytecode 已可用于正式工作负载，但没有满足默认切换的 CPU 收益门槛，因此 AST 保持默认。学习手册已同步当前版本、阶段状态、跨平台证据与风险结论。

## 9. 证据索引

- [语言规范](../../docs/HHY_V1.md)
- [Runtime 治理](../../docs/RUNTIME_GOVERNANCE.md)
- [性能预算](../../benchmarks/performance-budget.json)
- [v1.2.2 Release Notes](../../docs/releases/v1.2.2.md)
- [v1.3.0 Release Notes](../../docs/releases/v1.3.0.md)
- [v1.3.1 Release Notes](../../docs/releases/v1.3.1.md)
- [v1.3.2 Release Notes](../../docs/releases/v1.3.2.md)
- [下一阶段路线图](../2026-08-31-iteration-roadmap/HHY-NEXT-ITERATION-ROADMAP.md)
- [本报告网页版本](./HHY-LANGUAGE-HEALTH-REPORT.html)
