# HHY 语言状态报告 / HHY Language Status Report

报告日期 / Report date: 2026-08-31

发布基线 / Published baseline: HHY v1.1.8
证据提交 / Evidence commit: `43db191`

## 1. 发布摘要 / Release summary

HHY 核心语义保持稳定，Runtime 已建立资源与所有权治理，结构化诊断和编辑器
链路可用，四平台持续验证全绿。本期 4 项性能 workload 共采集 20 个样本，全部
通过绝对预算和启动归一化门禁。

HHY core semantics remain stable, Runtime resource and ownership governance is
established, structured diagnostics and editor tooling are operational, and all
four verification platforms are green. Four workloads produced 20 samples in
this report; every absolute and startup-normalized performance gate passed.

## 2. 总体结论 / Overall conclusion

| 维度 / Dimension | 当前结论 / Current conclusion |
| --- | --- |
| 语言语义 / Semantics | Pipe、Value、Stream、Error 与核心 callable contract 已冻结 / Core semantics and callable contracts are frozen |
| 兼容性 / Compatibility | 合法 v1 程序、诊断退出码和资源限制保持稳定 / Valid v1 programs, diagnostic exit codes, and resource limits remain stable |
| Runtime 健康度 / Runtime health | GC stress、ASan/UBSan、fuzz、资源上限与内部所有权治理形成门禁 / GC stress, sanitizers, fuzzing, limits, and ownership governance are gated |
| 性能 / Performance | 固定 workload、五次中位数、机器可读证据和阻断式预算已建立 / Fixed workloads, five-sample medians, machine-readable evidence, and blocking budgets are established |
| 生态边界 / Ecosystem | 优先使用 Process Extension Protocol，当前不开放 Native ABI / Process extensions remain preferred; no Native ABI is currently published |

## 3. 性能基线 / Performance baseline

环境 / Environment: GitHub Actions Ubuntu 24.04, Linux 6.17 Azure x86_64,
4 vCPU, Python 3.12.3. 每项连续运行 5 次，单位为毫秒。

| Workload | Median | Min–max | Budget | Used |
| --- | ---: | ---: | ---: | ---: |
| CLI `--version` | 4.001 ms | 3.948–5.065 ms | 100 ms | 4.0% |
| Basic Flow | 5.480 ms | 5.363–5.585 ms | 250 ms | 2.2% |
| Core Flow 100k | 45.020 ms | 44.297–46.172 ms | 500 ms | 9.0% |
| JSON Flow | 5.543 ms | 5.492–6.255 ms | 250 ms | 2.2% |

| Relative signal | Measured | Gate | Result |
| --- | ---: | ---: | --- |
| Basic Flow / CLI startup | 1.37× | ≤ 12× | Pass |
| Core Flow 100k / CLI startup | 11.25× | ≤ 20× | Pass |
| JSON Flow / CLI startup | 1.39× | ≤ 12× | Pass |

这些数据用于跟踪 HHY 自身性能回归，不作为跨语言排名。原始 20 个样本保存在
GitHub Actions performance-baseline artifact；预算位于
`benchmarks/performance-budget.json`，修改必须附带数据和原因。

## 4. 治理与风险 / Governance and risks

- Runtime 仍较集中，后续只能按 Value/Map、Stream、Codec、effectful resource、
  evaluator owner 的顺序逐职责迁移。
- 内部 C 边界区分 borrowed、managed-scanned、managed-atomic 与 native-owned；
  这些标注不构成公开 ABI。
- Native ABI 只有在至少两个真实集成证明进程协议不足时才重新评估。
- 本报告只在总体基线、测量方法或风险结论变化时更新，不随每个补丁版本复制。

- Runtime remains concentrated, so extraction proceeds one stable responsibility
  at a time: Value/Map, Stream, codecs, effectful resources, then evaluator owner.
- Internal C boundaries distinguish borrowed, managed-scanned, managed-atomic,
  and native-owned storage without creating a public ABI.
- A Native ABI is reconsidered only if at least two real integrations demonstrate
  that the process protocol is insufficient.
- This report changes when the overall baseline, methodology, or risk conclusion
  changes—not for every patch release.

## 5. 持续证据 / Continuous evidence

- GitHub Actions: <https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml>
- Runtime governance: [`docs/RUNTIME_GOVERNANCE.md`](../../docs/RUNTIME_GOVERNANCE.md)
- Performance budget: [`benchmarks/performance-budget.json`](../../benchmarks/performance-budget.json)
- Language specification: [`docs/HHY_V1.md`](../../docs/HHY_V1.md)
