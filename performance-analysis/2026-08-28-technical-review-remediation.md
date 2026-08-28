# HHY VM 技术评审全量修复报告

日期：2026-08-28
修复基线：`8bc7df9`（v1.1.2）
对应评审：`2026-08-27-technical-review/HHY-VM-TECHNICAL-REVIEW.md`

## 结论

评审列出的 2 个 P1、3 个 P2、3 个 P3 均已处理。两个发布阻断级 GC 问题已从所有点名路径移除，并在 Release suite 中用 `HHY_GC_STRESS=1` 的确定性强制回收测试覆盖。ASan/UBSan suite 独立覆盖内存与未定义行为；不把 ASan fake stack 与保守 GC 强制回收叠加作为根可达性 oracle。本轮没有改动用户工作区中与修复无关的图片文件。

## 逐项修复与证据

| ID | 修复 | 代码/测试证据 | 状态 |
| --- | --- | --- | --- |
| HHY-GC-001 | 新增 `rt_scanned_array_grow`、`rt_value_array_grow`、`rt_pointer_array_grow`，将 sort、collect、JSON array/object、CSV row 的托管引用缓冲迁入 BDWGC scanned storage；托管缓冲不再 `free` | `src/runtime.c`；`tests/run.sh` 在 advanced-flow、json-flow、csv-flow 上逐次分配强制 GC 并比较输出 | 已修复 |
| HHY-GC-002 | `Stream.seen` 改用 scanned Value array；`seen_times` 和哈希 slot 仍为不含指针的 native storage，关闭时分别按所有权释放/清引用 | `src/runtime.c: Stream`、`stream_next`、`stream_close` | 已修复 |
| HHY-PERF-001 | `distinct` 改为保持首次出现顺序的开放寻址哈希集合，碰撞后仍用 `equal_values` 校验；平均复杂度由 O(n²) 降为 O(n) | 本机 10k/20k 唯一 Int 均约 0.01s；规模翻倍未出现四倍增长 | 已修复 |
| HHY-PERF-002 | Map 增加构造期开放寻址索引，宽 Map 热查找走 hash+字节终检；JSON object 重复键检测改为增量哈希；小 Map 保留低常数线性路径 | `hash_key_bytes`、`map_build_index`、`map_lookup_n`、`json_parse_object` | 已修复 |
| HHY-TEST-001 | `process_snapshot` 检查 `/bin/ps` exit code，拒绝时返回 `ProcessError(HHY_PROCESS_SNAPSHOT)`；测试运行器先做能力探测，只跳过依赖真实进程快照的断言，并输出明确原因，CI 能力可用时仍强制验收 | 受限宿主输出 `SKIP: host denies /bin/ps...`；非零命令不再伪装为空快照 | 已修复 |
| HHY-ARCH-001 | 采用低风险渐进拆分：先建立带类型语义的 scanned/native buffer API 和 Map index 边界，消除本次事故所暴露的“靠调用者记忆”规则；没有进行高风险的一次性 Runtime 重写 | 三个具名 buffer API；测试中的 native `Value *` allocator 静态禁用规则 | 已修复（增量架构边界） |
| HHY-PROF-001 | JSON 增加 `cpu_sample_period_us`、`cpu_data_quality`、`warnings`；少于 10 个样本标为 `insufficient` | `src/profiler.c`；`tests/run.sh` 字段断言及 JSON 语法校验 | 已修复 |
| HHY-DOC-001 | 修正旧报告中“所有跨 GC 分配点”的过度断言，绑定原 commit 与实际覆盖范围；技术评审保留原文并增加修复状态链接 | `2026-08-27-runtime-hardening/02-test-report.md`、原技术评审顶部状态 | 已修复 |

## 最佳实践说明

- 承载 `Value` 或 GC 指针的数组统一进入 scanned storage；纯字节、时间戳、计数和 hash slot 使用 atomic/native storage。
- Grow 使用“新托管数组 + 已用元素复制”，不依赖普通 `realloc`，旧数组由 GC 回收。
- 哈希命中后仍执行完整 equality/长度/字节比较，避免 hash collision 破坏语义。
- 小 Map 少于 8 项不创建索引，兼顾常见 record 的空间和常数开销；宽 Map 才使用索引。
- GC 压力开关只由 `HHY_GC_STRESS` 显式启用，不改变普通生产执行策略。
- 测试跳过以实际宿主能力探测为依据，不按操作系统硬编码；有能力的 CI 仍执行原验收。

## 验证记录

```text
$ make test
all lexer, parser and runtime tests passed

$ make test-debug
all lexer, parser and runtime tests passed

$ HHY_GC_STRESS=1 ./build/hhy run tests/valid/advanced-flow.hhy
$ HHY_GC_STRESS=1 ./build/hhy run tests/valid/json-flow.hhy
$ HHY_GC_STRESS=1 ./build/hhy run tests/valid/csv-flow.hhy
输出分别与非压力运行完全一致（测试套件自动比较）。

$ git diff --check
无输出（通过）。

$ rg 'Value \*[^;=]*= hhy_(alloc|realloc)|Value \*[^;]*hhy_realloc' src/runtime.c
无匹配（测试套件也执行同一所有权守卫）。
```

首次 Debug 验证曾发现新增 Map 索引字段在旧反序列化构造路径可能未初始化；随后增加固定 magic 标记，只在 `map_build_index` 完整建索引后启用快路径。修复后 ASan+UBSan 全量测试通过。该过程保留在报告中，作为 sanitizer 实际发现并阻止未定义行为进入提交的证据。

## CI 验收条件

GitHub Actions 应至少完成仓库现有跨平台工作流，并执行 Release 测试。CI 成功链接或 run ID 在浏览器触发完成后补充到本报告。
