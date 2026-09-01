# HHY VM Technical Review

日期：2026-08-30
审查版本：`1.1.4`，commit `9392e00`（`v1.1.4-7-g9392e00-dirty`）
对照基线：`../2026-08-27-technical-review/HHY-VM-TECHNICAL-REVIEW.md` 与同目录三份后续修复/验证报告
审查方式：源码静态审查、Release 全量测试、ASan+UBSan Debug 全量测试、Crawler/SiteGraph fixture 验收、GC stress、Profiler 基准
变更范围：仅新增本报告与修复设计；未修改 Runtime、语言语义或测试

> 工作区说明：HEAD 与 `origin/main` 均为 `9392e00`，但审查时存在未提交的网站响应式修改、推广材料和图片。本报告的 VM/Runtime 结论以已提交 HEAD 为基线；移动旧审查证据文件属于本轮用户明确要求的目录整理。

## Executive Summary

HHY v1.1.4 的核心 Runtime 相比 2026-08-27 基线明显更可靠。旧报告中的 scanned/native buffer 混用、`distinct` 平方复杂度、宽 Map 查找、process snapshot 假成功和 Profiler 低样本误导均已有真实代码与测试修复。本轮 Release、ASan+UBSan、官方 Crawler 和 SiteGraph Auditor 验收全部通过；20,000 个动态 String 在逐分配强制 GC 下也保持正确。

本轮没有确认 P0。确认 1 个 P1、2 个 P2、3 个 P3。发布阻断项位于官方 Crawler 的安全配置默认值：文档与示例声称 Crawler 默认禁止私网，但 `engine.hhy` 直接透传可缺失的配置字段；字段缺失或拼错时得到 Null，而通用 HTTP 会把 Null 解释为 `allow_private_networks: true`。这会让面向不可信 URL 的 Spider 静默失去连接级 SSRF 防护。

另一个重要回归是 Map 哈希索引元数据直接嵌入 `Map`，而 `Map` 又内嵌在每一个 `Value` 的 union 中。Fibonacci(30) 的调用与分配次数几乎未变，但累计分配从旧报告的约 215.4 MB 增至 258.5 MB，约增加 20%。宽 Map 查找性能改善是成立的，但实现方式让所有非 Map Value 也承担布局成本。

| 维度 | 评分 | 判断 |
| --- | ---: | --- |
| Production readiness | 80/100 | Runtime 主路径成熟；官方安全 Crawler 必须先改为 fail-closed |
| Language semantics | 88/100 | 语义稳定，新增 URL callable 与既有严格类型方向一致 |
| VM correctness | 84/100 | AST evaluator 主路径、Resolver、Stream 与系统对象测试完整 |
| Memory management | 82/100 | 旧 GC root 问题已封闭；Value 布局出现可量化空间回归 |
| GC | 88/100 | scanned/native 边界明确且 stress 正确；矩阵仍未覆盖 debounce/dynamic distinct |
| Performance | 79/100 | distinct/Map 算法改善；Value 膨胀和 Crawler O(n²) 状态管理需处理 |
| Error handling | 87/100 | process capability 与 URL 错误分类清楚；Crawler 配置缺少前置诊断 |
| Test quality | 87/100 | 66 valid、26 invalid、6 acceptance；Release/Debug 全绿，安全默认测试缺失 |
| Portability | 85/100 | 三平台 CI 设计完整；本机 macOS arm64 验证通过 |
| Maintainability | 64/100 | `runtime.c` 已达 6,126 行、196 个 static function，仍高度集中 |

## Architecture Delta

执行架构仍是 Lexer → Pratt Parser → Checker → Resolver → AST Evaluator → Runtime/Stream/System；不存在 bytecode compiler、instruction pointer 或 operand stack，因此仍不应称为传统字节码 VM。

相对 2026-08-27 的主要变化：

- `Value` 的 Map 表示增加开放寻址索引元数据；宽 Map lookup 和 JSON duplicate detection 改为平均 O(1)。
- `Stream.distinct` 增加开放寻址 slot table，保持首次出现顺序。
- Value/pointer 临时数组迁入 BDWGC scanned storage，并增加 `HHY_GC_STRESS`。
- Profiler JSON 增加采样周期、质量与 warning。
- process snapshot 检查 `/bin/ps` exit code，受限环境明确 skip。
- 新增 `url_resolve` 与连接地址级私网阻断，用于官方静态 Crawler。
- `runtime.c` 从旧审查的 5,877 行/约 186 个 static function 增长至 6,126 行/196 个。

## P0

本次没有发现能够复现或由源码直接证明的 P0。

## P1

### HHY-CRAWLER-SEC-001：Crawler 的 SSRF 防护在配置缺失时 fail-open

**Severity:** P1
**Category:** Security / Configuration / Documentation contract
**Status:** Confirmed by source data flow

**Location:**

- `practical-projects/my-crawler/lib/engine.hhy:26-31`
- `src/runtime.c:3684,3704-3705,3724`
- `docs/KNOWN_LIMITATIONS.md:29`
- `practical-projects/my-crawler/README.zh-CN.md:21-25`

**Problem:** Crawler 把 `config.allow_private_networks` 原样传给 `http.get`。HHY 对不存在的 Map member 返回 Null；通用 HTTP 对 Null 使用 `true` 作为兼容默认值。因此缺少字段、字段拼错或旧配置升级时，Crawler 不报错而是允许 loopback、私网和 link-local。

**Expected:** 官方面向不可信 URL 的 Crawler 无论配置是否完整都默认 fail-closed；只有明确布尔值 `true` 才允许本机 fixture。

**Actual:** 安全性依赖每份配置都正确包含 `allow_private_networks: false`，与“Crawler 默认启用保护”的文档表述不一致。

**Impact:** 攻击者控制 seed、redirect 或页面链接时，配置疏漏可能允许探测或访问内部 HTTP 服务。

**Recommendation:** 在 Crawler 配置归一化层生成明确 Bool：`allow_private_networks: config.allow_private_networks == true`；生产模板仍写 false，本地 fixture 显式 true。对未知/拼错字段、缺失边界列表和非法 limit 做启动期校验。新增“省略字段仍阻止 127.0.0.1”和“显式 true 才允许 fixture”的验收。

## P2

### HHY-PERF-003：Map 索引字段扩大所有 Value 的布局与分配流量

**Severity:** P2
**Category:** Performance / Runtime object layout

**Location:** `src/runtime.c:89-124`、`1453-1481`

`Map` 新增 `slots`、`slot_count`、`index_magic`，并继续作为 `Value` union 的内嵌成员。union 大小由最大成员决定，因此 Int、Bool、String、Range 等所有 Value 也承担 Map 索引字段的空间成本。

Profiler 对比：

| Fibonacci(30) | 2026-08-27 审查 | 2026-08-30 本轮 | 变化 |
| --- | ---: | ---: | ---: |
| Function calls | 2,692,537 | 2,692,537（语义相同） | 0% |
| Allocations | 4,038,978 | 4,038,971 | 近似不变 |
| Allocated bytes | 约 215.4 MB | 258,497,607 B | 约 +20% |
| Heap peak | 约 2.18 MB | 2,175,032 B | 近似不变 |
| Wall | 256.6 ms | 272.6 ms | 单次约 +6% |

分配次数不变而字节显著增加，与 Value 布局扩大一致。单次 wall 数据只作诊断，不作为跨环境排名。

**Recommendation:** 将 Map 表示改为 `MapStorage *`/对象指针，使 Value union 只保存指针；Map 的 count、arrays、index 放入独立 GC-scanned header。对小 Map 继续不建 slot table。变更前后用 `sizeof(Value)` 静态断言、Fibonacci 分配字节和宽 Map lookup 三组基准共同验收，避免只优化 Map 而损伤全局 Value 密度。

### HHY-CRAWLER-PERF-001：Frontier、pages 与 fingerprint 使用不可变 List 形成 O(n²)

**Severity:** P2
**Category:** Performance / Application architecture

**Location:** `practical-projects/my-crawler/lib/engine.hhy:53-55,61-66,81-96`

`contains(fingerprints, ...)` 线性查找，随后 `append` 创建新 List；`frontier`、`pages` 和 `fingerprints` 都在循环中重复 append。处理 n 个唯一 URL 时，去重比较和集合复制均累计为 O(n²)。Runtime 已有高效 `distinct`，但 Crawler 需要跨深度、增量 membership，当前语言缺少可复用的 hash-backed Set builder。

默认 `max_pages=50` 时影响有限，但用户可配置更大边界；这与 v1.1.4 强调 Frontier 和指纹去重的定位不匹配。

**Recommendation:** 近期对 Crawler 配置设置并验证保守硬上限；中期提供保序 `Set`/`set_add`/`set_contains` 或受控 mutable collection，使增量 fingerprint membership 平均 O(1)。增加 1k/5k/10k 合成链接图规模倍增基准。

## P3

### HHY-TEST-002：GC stress 矩阵没有覆盖动态 String distinct 与 debounce

`tests/run.sh:45-51` 只在 `advanced-flow`、JSON、CSV 上启用 `HHY_GC_STRESS`；`advanced-flow` 的 distinct 仅有四个 Int。`tests/valid/debounce.hhy` 在普通模式运行，没有强制 GC。旧 P1 的实现已正确迁入 scanned Value storage，本轮额外执行 20,000 个动态 `encode_json(n)` String distinct，在普通/GC stress 下均输出 20,000；但该证据尚未固化到仓库测试。

建议增加专用 `gc-distinct-dynamic.hhy`，并让可注入事件源的 debounce 在逐 pull GC 下验证动态 Map/FileEvent。

### HHY-ARCH-002：Runtime 单文件继续增长，旧“架构修复”只封闭了局部所有权

`src/runtime.c` 当前 6,126 行、196 个 static function，继续同时承担 Value、Map、GC、Eval、Stream、HTTP、URL、process、codec、module、REPL 等职责。2026-08-28 修复报告将旧架构项标为“已修复（增量架构边界）”对 scanned buffer 事故是成立的，但不代表单文件职责问题消失。

建议优先抽出 `runtime_value/map`、`runtime_stream`、`runtime_http_url` 三个边界，保持行为不变并用现有全量测试护航。

### HHY-EVIDENCE-001：跨平台成功证据绑定旧 commit，当前 HEAD 尚无本轮证据快照

现有修复报告引用的是 commit `ffafced` 的 GitHub Actions。当前 HEAD `9392e00` 之后包含 v1.1.4 crawler、安装器、网站与文档变化。CI workflow 设计覆盖 macOS arm64、Linux arm64、Linux x86_64，但本轮没有重新读取当前 HEAD 的远端 run/artifact，因此不能把旧 run 当作当前提交的跨平台证明。

建议每次技术审计记录 HEAD、workflow run URL、三平台 job conclusion 与 artifact digest；未验证时明确写“设计存在、远端状态未审计”。

## Verification Record

### Tests

```text
$ make test
all lexer, parser and runtime tests passed

$ make test-debug
all lexer, parser and runtime tests passed

$ ./practical-projects/my-crawler/self-test.sh
Pages 3 / 3 Records 3 Failures 0
HHY Collector Framework self-test passed

$ ./practical-projects/sitegraph-auditor/self-test.sh
healthy: Pages 4 / 4, Edges 5, Findings 0
risky: Pages 1 / 2, Edges 2, Findings 3
SiteGraph Auditor self-test passed
```

首次受限执行时，本地 HTTP fixture 因沙箱禁止 bind 失败；允许仅回环 fixture 后全绿。这是审计环境限制，不是 Runtime failure。`/bin/ps` 在沙箱内继续按设计输出明确 SKIP。

### GC stress

```text
20,000 dynamic encode_json String values
normal:     20000
GC stress:  20000
```

### Distinct scaling

| Unique Int count | Wall | Allocated | Allocations | CPU quality |
| ---: | ---: | ---: | ---: | --- |
| 100,000 | 12.5 ms | 44,958,107 B | 172 | insufficient (7 samples) |
| 200,000 | 23.4 ms | 89,906,843 B | 174 | usable (16 samples) |
| 400,000 | 50.4 ms | 179,804,315 B | 176 | usable (36 samples) |

规模翻倍约 1.87×、2.15×，没有恢复为 O(n²)。Profiler 的质量字段按预期区分低样本结果。

## Release Gate

当前 Runtime 本身可进入 v1.1.4 维护发布；官方“安全静态 Crawler”不应在 HHY-CRAWLER-SEC-001 修复前继续宣称无条件默认阻止 SSRF。

最低门槛：

1. Crawler 配置缺失/拼错 `allow_private_networks` 时仍强制 false，只有显式 true 才放行。
2. 增加省略字段、loopback、redirect-to-loopback 三个 fixture 验收。
3. 为 Value/Map 布局建立 `sizeof(Value)` 与 allocated-bytes 回归基线。
4. 将 dynamic String distinct 与 debounce 纳入 GC stress。
5. 对当前 HEAD 记录三平台 CI run 与 artifact digest。

完成 P1 后，Production readiness 可恢复至约 85/100。Map 布局与 Crawler O(n²) 可在不改变语言语义的 v1.1.x 维护周期渐进修复。
