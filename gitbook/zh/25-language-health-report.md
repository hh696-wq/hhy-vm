# 25. HHY 语言状态报告 · 2026-08-31

发布 HHY 当前语义、Runtime、性能与工程质量状态，包含可复核的 CI 实测数据。

## 25.1 发布摘要

{% hint style="info" %}
HHY 当前语言基线为 v1.1.8。核心语义保持稳定，Runtime 已建立资源与所有权治理，结构化诊断和编辑器链路可用，四平台持续验证全绿；本期实测未触发任何性能回归预算。
{% endhint %}


| 报告维度 | 回答的问题 | 当前结论 |
| --- | --- | --- |
| 语言基线 | 核心语义是否稳定 | Pipe、Value、Stream、Error 与核心 callable contract 已冻结 |
| Runtime 健康度 | 资源、内存与取消边界是否可靠 | 具备资源上限、GC stress、sanitizer、fuzz 与显式所有权治理 |
| 性能 | 性能是否可测且受控 | 固定工作负载、五次采样中位数、机器可读证据和阻断式预算已建立 |
| 工程治理 | 变化是否可审计 | 四平台 CI、分层门禁、版本一致性和发行证据已形成闭环 |


## 25.2 本期数据概览

| 数据项 | 结果 | 证据口径 |
| --- | --- | --- |
| 核心 callable | 96 | Runtime Callable Contract Registry |
| 网站 HHY 示例 | 47 个通过 | 每次生产构建执行 Parser/Checker 验证 |
| 完整规范代码块 | 21 个通过 | docs/HHY_V1.md 文档检查 |
| 持续验证平台 | 4 个 | macOS arm64、Linux arm64、Linux x86_64、Windows x86_64 |
| 性能 workload | 4 项 / 20 次采样 | 每项连续 5 次，使用中位数 |
| 完整实战项目 | 6 个 | 端到端 acceptance 与稳定退出码 |


## 25.3 总体基线与兼容性

| 基线 | 稳定承诺 | 验证方式 |
| --- | --- | --- |
| 语言语义 | 不引入第二套 Pipe、Stream 或 Error 模型 | 规范示例、Parser/Checker fixtures 与合法程序回归 |
| Callable contract | 名称、arity、effect、lazy、cancellable 和 threading 可机器读取 | Contract Registry JSON 与 96 项 contract 一致性检查 |
| 诊断 | CLI 文本与 JSON/LSP 使用同一 Core 检查路径 | 诊断 schema 与 LSP 协议测试 |
| 扩展边界 | 第三方能力优先走 Process Extension Protocol | 清单完整性、Protocol 1 与官方扩展验收 |
| C ABI | 当前不公开 Runtime 内部 ABI | 只有真实集成证据证明进程协议不足时才重新决策 |


最新采样点为 v1.1.8。v1.1.6–v1.1.8 提供了支撑本报告的测试、编辑器和 Runtime 治理能力，但它们不是三份彼此独立的总体报告。


## 25.4 性能实测

采样环境：GitHub Actions Ubuntu 24.04，Linux 6.17 Azure x86_64，4 vCPU，Python 3.12.3；提交 43db191，HHY v1.1.8。每项连续执行 5 次，单位为毫秒。


| Workload | 中位数 | 最小–最大 | 绝对预算 | 预算占用 |
| --- | --- | --- | --- | --- |
| CLI --version | 4.001 ms | 3.948–5.065 ms | 100 ms | 4.0% |
| Basic Flow | 5.480 ms | 5.363–5.585 ms | 250 ms | 2.2% |
| Core Flow 100k | 45.020 ms | 44.297–46.172 ms | 500 ms | 9.0% |
| JSON Flow | 5.543 ms | 5.492–6.255 ms | 250 ms | 2.2% |


| 相对指标 | 实测 | 门槛 | 结果 |
| --- | --- | --- | --- |
| Basic Flow / CLI 启动 | 1.37× | ≤ 12× | 通过 |
| Core Flow 100k / CLI 启动 | 11.25× | ≤ 20× | 通过 |
| JSON Flow / CLI 启动 | 1.39× | ≤ 12× | 通过 |


{% hint style="info" %}
这些数字用于跟踪 HHY 自身回归，不用于宣称跨语言性能排名。原始样本保存在对应 GitHub Actions performance baseline artifact 中；预算修改必须附带数据与原因。
{% endhint %}


## 25.5 治理结论与后续观察

- 总体状态：语言语义稳定，Runtime 与工具链进入证据驱动的渐进治理阶段。
- 当前主要风险：Runtime 仍较集中，后续只能按单一稳定职责逐步拆分。
- 性能策略：先保持语义与资源边界，再依据 Profiler 和 benchmark 决定优化。
- 生态策略：优先完善进程扩展工具链，不提前承诺 Native ABI。
- 更新规则：当基线、测量方法或总体风险结论变化时更新本报告，而不是每个补丁版本都复制一份。


[查看持续验证证据](https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml)

四平台构建、sanitizer、GC stress、fuzz、性能基线、真实项目和发行包证据。
