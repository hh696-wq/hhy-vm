# v1.6.3：GC 与调度边界

本阶段交付本地诊断、真实项目需求审计和条件式架构决策。保留 Boehm conservative GC 与现有有界、按输入顺序输出的进程 parallel；不表示现有方案已经满足未知的生产 SLA，也不表示分代 GC 或 async 永远没有收益。

## 独立 GC 诊断

```sh
make build/hhy-resource-probe
build/hhy-resource-probe build/gc.json bytecode benchmarks/gc-retention.hhy
HHY_PROBE_LIVE_CONTEXT=1 build/hhy-resource-probe build/live.json bytecode benchmarks/gc-retention.hhy
make test-runtime-resources
python3 scripts/evaluate-runtime-resources.py --output build/runtime-resources.json
python3 scripts/profile-runtime-workloads.py --output build/runtime-workload-resources
```

探针是单独的诊断可执行文件，使用本仓库已有 private Runtime harness 构建方式；生产 CLI/library 不安装 GC 回调，没有新增默认开关、分配或采样开销。探针读取当前链接 Boehm 的公开事件及统计接口，不修改收集策略。已有事件回调非空时拒绝接管。

GC 回调在 collector lock 内执行，只读取单调时钟、判断进程身份并写入固定大小数值表；不分配、不打印、不调用 GC 查询。只收集父进程事件；fork worker 的 GC 不能归入父进程。退出后先恢复回调，再进行诊断用最终强制回收。

- allocation rate：collector 的累计分配字节差 / 测量窗口。包括父进程 Runtime 分配，不包含 native malloc 或 worker 分配；不等于 Profiler 的请求字节计数。
- collection duration：GC_EVENT_START → END。pause：PRE_STOP_WORLD → POST_START_WORLD，包含停/启世界动作。每窗口最多保存 4,096 个样本；total/max 继续累计，overflow 明示，不把丢弃后的样本分位数当作全量分位数。
- CLI 模式执行窗口包含 Runtime 自己的初始 GC，不含探针加载 Application 的准备阶段；可分别看到 collection/pause 总量，短任务不得把初始回收成本误解为稳态成本。
- final forced collection 在窗口外单独计时。默认报告 Runtime 退出后的 retained estimate；live-context 模式在 Context 和词法环境仍可达时测量，可暴露生命周期保留。
- retained estimate 来自 GC_get_memory_use，是 conservative 回收后的保留量，不是精确对象 live-set，也不证明所有保留对象有业务用途。heap size、分配量、RSS 是不同指标。
- max_rss_native_units 保留 getrusage 原生单位：macOS 为字节，Linux 通常为 KiB。children user/system CPU 仅是 getrusage 汇总，不能替代 worker Heap/pause 画像。
- promotion_bytes 和 write_barrier_ns 为 null：当前接口不提供对应成本，不能填 0 或推导“没有成本”。incremental 字段记录实际 collector mode，不偷偷启用实验模式。

探针独占诊断进程的回调槽，不是面向任意多线程嵌入宿主的可并发 profiler API。失败路径保持原程序退出状态；不把诊断数据推广为多平台性能准入。

## GC 决策

新增 gc-retention 合成负载同时包含保留数据和临时 Map/Closure 分配，用于识别回收压力；它不是生产预算。Context 保留量较大时，先检查 reachable roots 与保留生命周期，再判断收集算法是否是主因。

源码中的 Runtime Stream 链、关闭 Stream 未清空的 Env/Value 字段，以及 escaped Env 都可能延长对象可达时间。闭包确实共享词法 Env；精确标记器也不能回收仍然可达的对象。本轮记录该候选问题，不通过清空尚未完成语义审计的 roots 制造“优化收益”。

未提供生产 pause/live-set 预算，没有基于两个以上生产部署的 GC 瓶颈证据。因此不进入 precise roots、分代或增量 GC 重写；现有 GC 语义和配额保持。这个决定是保守的条件决策，不是“已证明满足所有预算”。

## Scheduler 需求审计

| 项目 | 已有机制与需求证据 |
| --- | --- |
| DataFlow ETL | lib/enrich.hhy 使用 parallel(4) 并发 HTTP enrichment，单请求 timeout(3s) |
| Multi-API Collector | lib/sources.hhy 使用配置并行数；timeout(10s)、retry、attempt 处理多来源分页失败 |
| FlowGuard | 并行执行质量命令和 HTTP 健康检查，self-test 覆盖成功/失败门禁 |
| SiteGraph Auditor | 按深度推进、有界并发的 Frontier，自测覆盖链接发现、边界和失败请求 |

这些现有 fixture 验证当前机制可以完成项目中的任务；没有证明至少两个真实生产项目因现有机制不足而需要 async runtime。浏览器依赖的 my-crawler 未在本次启用，不用它补足证据数量。

parallel 默认受 max_processes=16 与 max_parallelism=16 双重限制，先等待 jobs[0] 再交付，保留输入顺序。慢首项会造成队头等待，这是当前顺序语义及实现的成本；测量 workers=1/2/4 的相同延迟负载，不把改成完成顺序当作透明优化。
取消评估在 Runtime 初始化完成并进入工作后注入 SIGINT，测量信号到父进程退出的时间；在兜底清理前用进程组检查 worker 已全部消失。记录 15 次样本与观测最大值，不给出硬实时保证。

关闭 worker 的现有逻辑最多先轮询 SIGTERM 50×10ms，再 SIGKILL 并 waitpid；OS 调度和阻塞状态使这不是整个操作的严格最坏墙钟界限。维持已有 Error、取消和配额模型，不创建第二套调度语义。

## 本地验收与后续门槛

Release / ASan / UBSan 探针差分验证、存活 Context 保留量测试、每负载 15 次窗口采样、60 次取消注入、90 次有序并行场景及六项目双引擎自测构成本地证据。实际数量与原始数据以本次开发记录为准。

机器可读决策位于 benchmarks/vm-gc-scheduler-policy.json。生产 pause/concurrency SLA、worker GC 画像、跨平台性能与 closed Stream roots 的进一步优化均不在本次完成声明内。只有相关预算和真实场景证据成立，才进入后续优化实现。
