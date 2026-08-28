# HHY VM Technical Review

> 修复状态（2026-08-28）：本报告列出的 2 个 P1、3 个 P2、3 个 P3 已完成代码、测试或证据治理修复。逐项变更、命令与结果见 `../2026-08-28-technical-review-remediation.md`。原始审查正文保留，作为修复前基线，避免事后改写证据。

日期：2026-08-27
审查版本：`1.1.2`，commit `8bc7df9`
审查方式：源码静态审查、Release/ASan+UBSan 测试、CLI 行为检查、内建 Profiler 基准
变更范围：仅新增本报告，未修改 Runtime、语言实现或测试

## Executive Summary

HHY v1.1.2 已经是一套功能完整、文档和测试意识较强的 AST 解释器，而不是字节码 VM。Lexer、Pratt Parser、Checker、slot resolver、AST evaluator、Flow/Stream、系统 Runtime、保守 GC、Profiler 和进程扩展协议均有真实实现。CLI 退出码、Unicode 字符串语义、整数溢出、惰性 Stream、资源限制、原子文件输出、并行 worker 回收及跨平台 CI 等方面明显优于普通实验性解释器。

当前版本不建议直接以“无条件 production-ready”评价。主要阻断项不是语法或基础执行，而是几处仍由普通 `malloc/realloc` 保存 GC-managed `Value` 的路径。Boehm GC 不扫描这些数组；当数组成为对象唯一引用且后续分配触发 GC 时，仍在使用的 String/List/Map/系统对象可能被提前回收。这与此前 runtime-hardening 报告中“所有跨 GC 分配点的 Value 临时数组已改为托管内存”的结论不一致。

本次未发现可稳定证明的 P0。确认 2 个 P1、3 个 P2、3 个 P3。优先修复 GC 根问题并加入强制 GC 回归后，项目才适合进入 v1.1.2 发布候选验收。

| 维度 | 评分 | 判断 |
| --- | ---: | --- |
| Production readiness | 68/100 | 核心能力完整，但 GC 根生命周期问题需先修复 |
| Language semantics | 87/100 | 规范清晰，Bool、数字、String、Flow 语义一致性较好 |
| VM correctness | 76/100 | 实际为 AST evaluator；主路径成熟，聚合路径仍有生命周期风险 |
| Memory management | 63/100 | Runtime 根链与多数对象处理良好，但普通堆中的 `Value` 指针不可见 |
| GC | 60/100 | 有限额、观测和根链修复；仍存在多处漏根 |
| Performance | 73/100 | 调用帧优化有效；递归与部分集合操作仍昂贵 |
| Error handling | 84/100 | 类型、错误码、源码位置和退出码整体清楚 |
| Test quality | 82/100 | 65 valid、26 invalid、6 acceptance；缺少确定性 GC 压力注入 |
| Portability | 83/100 | 三平台原生 CI；平台 API 有明确条件分支与限制说明 |
| Maintainability | 62/100 | `runtime.c` 5,877 行、约 186 个静态函数，职责过度集中 |

## Architecture Overview

实际执行链为：

```text
.hhy source
    ↓  src/common.c: HhySource
Lexer
    ↓  src/lexer.c, include/hhy/token.h: HhyTokenList
Pratt Parser
    ↓  src/parser.c, src/ast.c: HhyNode tree
Static Checker
    ↓  src/checker.c, src/contracts.c
Slot Resolver
    ↓  src/resolver.c: lexical depth / binding slot / frame capacity
AST Evaluator（无 Compiler、无 Bytecode）
    ↓  src/runtime.c: eval / exec_node / call_value
Runtime Value + Env + Function/Closure
    ↓  Value, Binding, Env, Module
Flow / Stream
    ↓  Stream state machine / pull-based stream_next
System Runtime
    ↓  file/process/http/watch/json/csv/regex/extensions
BDWGC managed heap + explicit native-resource teardown
```

主要生命周期：主源码由 CLI 加载并持有到执行结束；AST token 借用源码切片；Resolver 将可静态确定的绑定写入 AST；函数/闭包保存 AST、源码与 Env；逃逸 Env 由 GC 管理，未逃逸调用帧进入复用链；Stream 由 Runtime 所有权链保活并在 teardown 显式关闭；Module 保留自己的源码、token、AST 和环境直到 Runtime 释放。

关键调用链：

```text
main → process_file → hhy_lex → hhy_parse → hhy_check / hhy_run_program
     → hhy_profile_program → hhy_resolve_slots → runtime_core_environment
     → exec_node ↔ eval → eval_call → call_value
     → builtin / stream_value → stream_next → stream_close
     → runtime_release
```

不存在 bytecode instruction、instruction pointer、operand stack、call-frame stack 或 upvalue opcode，因此“逐指令 stack effect”不适用于当前实现。当前闭包通过 `Function.closure → Env → Binding.Value` 实现。

## P0

本次没有发现能够复现或由源码直接证明的 P0。

## P1

### HHY-GC-001：物化、JSON 与 CSV 临时数组不是 GC roots

**Severity:** P1
**Category:** Correctness / GC / Memory
**Status:** Confirmed by ownership analysis；运行时表现可能依赖 GC 时机

**Location:**

- `src/runtime.c:2425-2446`, `stream_collect`
- `src/runtime.c:2826-2843`, `json_parse_array`
- `src/runtime.c:2846-2885`, `json_parse_object`
- `src/runtime.c:3243-3281`, `csv_parse_row`
- 同类审查点：`src/runtime.c:1247-1284`, `stable_sort_values`

**Problem:** 这些函数使用 `hhy_alloc/hhy_realloc`（普通 C heap）保存包含 GC 指针的 `Value`、String key 或两者。Boehm GC 不扫描普通 `malloc` 内存。

**Evidence:** `stream_collect` 将每个 Stream item 放进普通 `temporary`，直到结束才调用 `list_new()`。若 item 只被该数组引用，后续 `stream_next`、closure 或最终 `list_new` 的托管分配可触发 GC。JSON array/object 和 CSV row 也在普通数组中累计刚创建的托管 String/List/Map，再进行更多托管分配。`stable_sort_values` 的临时归并数组同样保存 `Value`；其当前比较循环通常不分配，但取消错误路径和未来比较器演进会使约束脆弱。

**Reproduce:** 建议增加测试专用的 `gc_collect()` hook 或小 heap build，在每次向临时数组写入后强制 `GC_gcollect()`：

```hhy
range(0, 50000)
    |> map { n -> { key: "item-{n}", nested: [n, "value-{n}"] } }
    |> collect
    |> length
    |> print
```

并分别构造 50,000 个嵌套 JSON object 与宽 CSV row。当前公开语言没有显式 GC API，因此普通运行不保证每次复现；源码的 root 可达性已足以确认缺陷。

**Expected:** 临时聚合期间，所有尚会使用的托管对象都可被 GC 扫描。

**Actual:** 对象可能只被普通 heap 数组引用，对 collector 不可见。

**Cause:** `hhy_alloc/hhy_realloc` 与 `rt_alloc` 的所有权语义只靠调用者记忆，没有类型或 API 约束；早前只修复了 sort/group 主物化数组，未覆盖 collect、JSON、CSV 等同类路径。

**Impact:** 非确定性错误值、损坏输出、崩溃或跨平台 Release-only 失败；嵌套对象、大输入和频繁 GC 时风险最高。

**Recommendation:** 所有承载 `Value` 或 GC 指针且跨越任一 `rt_alloc` 的缓冲改用可扫描 GC 内存；纯计数、hash slot、字节缓冲继续用 atomic/普通内存。把 API 明确拆成 `rt_value_array_*`、`rt_pointer_array_*` 与 `native_buffer_*`，避免再次混用。

**Regression Test:** 在测试构建加入 deterministic GC stress hook，覆盖 collect、JSON array/object、CSV row、sort/group，每次追加元素后强制回收并验证嵌套 String/List/Map 内容。

### HHY-GC-002：`distinct` 与 `debounce` 的跨调用状态不被 GC 扫描

**Severity:** P1
**Category:** Correctness / GC / Stream
**Status:** Confirmed by ownership analysis

**Location:** `src/runtime.c:2215-2229`、`2232-2265`；释放位于 `1700-1760` 附近

**Problem:** `Stream.seen` 通过 `hhy_realloc` 分配并跨多次 `stream_next()` 保存 `Value`。它不是短期 scratch buffer，而是 Stream 的长期状态；其中 String/Path 或 debounce Map 可能是其底层对象的最后引用。

**Evidence:** 每次读取下一个 upstream item 都可能分配；`distinct` 还会不断扩容，`debounce` 会保存 FileEvent/Map。`Stream` 本身在 GC heap 且可达，但 collector 看到的只是指向普通 heap 的 `seen` 指针，不会递归扫描普通 heap 内容。

**Reproduce:** 使用 GC stress build，让惰性源每次生成唯一动态 String，经 `distinct` 后完整 collect；另用大量动态 FileEvent/Map 经 `debounce`，在每次 pull 间强制 GC。

**Expected:** 已见键和事件在 Stream 关闭前保持有效。

**Actual:** `seen` 中的对象引用可能失效。

**Cause:** 将含 GC pointer 的持久状态当作普通可释放数组管理。

**Impact:** 去重结果错误、比较时读取失效字符串、watch/debounce 不稳定或崩溃。

**Recommendation:** `seen` 使用 GC-scanned storage；`seen_times` 仍可用 atomic/native storage。关闭 Stream 时无需 `free` GC 数组，只清除引用，或使用明确支持 scanned realloc 的封装。

**Regression Test:** 10k–100k 动态 String distinct 与 FileEvent debounce 的逐项强制 GC 测试。

## P2

### HHY-PERF-001：`distinct` 为 O(n²)，与已优化的 `group_by` 不一致

**Severity:** P2
**Category:** Performance / Semantic consistency

**Location:** `src/runtime.c:2215-2228`

**Problem/Evidence:** 每个新值线性扫描全部 `seen`。n 个唯一值需要约 n(n-1)/2 次相等比较；String 比较还与长度相关。`group_by` 已有开放寻址 hash，说明 Runtime 已具备一致的 scalar hash 语义。

**Reproduce:** 对 10k、20k、40k 个唯一 Int/String 执行 `stream |> distinct |> collect`，观察耗时接近四倍增长。

**Expected:** hashable scalar distinct 平均 O(n)。

**Actual:** O(n²)。

**Recommendation:** 复用 `hash_scalar + equal_values` 的开放寻址索引，并保留首次出现顺序。

**Regression Test:** 规模倍增基准和 Int/Float、±0、String/Path 的 hash/equality contract 测试。

### HHY-PERF-002：Map 查找与重复键检测均为线性/平方路径

**Severity:** P2
**Category:** Performance / Runtime Object System

**Location:** `src/runtime.c:1409-1428`；Map literal 构造约 `src/runtime.c:5070-5100`

**Problem:** Map/record member lookup 逐键扫描；Map literal 每插入一个键又扫描全部前项，构造为 O(n²)。JSON object duplicate 检测也为 O(n²)（`2864-2869`）。

**Impact:** 宽 JSON/record、ETL 字段映射和热循环属性访问成本快速增长。现有 resolver 只优化变量绑定，不优化 Map key。

**Recommendation:** 保持插入顺序数组作为迭代表示，同时增加惰性或构造期 hash index；所有查找最终仍做长度与字节相等校验。

**Regression Test:** 1k/10k/100k 宽 Map 的构造、末尾 key lookup、overwrite/pick/require 基准。

### HHY-TEST-001：测试依赖宿主 `ps`，受限运行环境会产生假失败

**Severity:** P2
**Category:** Test reliability / Portability

**Location:** `src/runtime.c:3700-3735`；`tests/valid/logical-record-types.hhy`；`tests/run.sh:749-763`

**Problem:** 全量测试无条件要求 `processes |> first` 得到 Process。当前受限 macOS 环境中 `/bin/ps` 返回“operation not permitted”，`command_run` 仍形成空 snapshot，测试最终以 Null/Null 失败。

**Evidence:** Release 与 ASan+UBSan 均稳定输出预期前十行，末尾为 `Null\nNull`；直接执行 `/bin/ps` 被宿主拒绝。该结果是环境约束，不足以证明 HHY 的 process parser 有缺陷，但测试报告只显示逻辑记录错误，掩盖了真实原因。

**Expected:** 若宿主禁止 process snapshot，测试明确 skip 或 Runtime 返回可诊断的 ProcessError。

**Actual:** 空列表被当作成功 snapshot，随后 `first` 返回 Null，聚合断言模糊失败。

**Recommendation:** 将 process snapshot 能力探测与类型序列化测试拆开；CI 保留真实 Process 验收，受限环境允许带原因 skip。Runtime 可区分“命令成功但确实无进程”和宿主拒绝。

**Regression Test:** 注入 `ps` exit status/empty output，验证错误分类与测试诊断。

## P3

### HHY-ARCH-001：Runtime 单文件职责过度集中

**Severity:** P3
**Category:** Maintainability / Architecture

**Location:** `src/runtime.c`（5,877 行，约 186 个 static function）

该文件同时实现 Value、Env、调用、GC/limits、Stream、parallel、files、process、HTTP、JSON、CSV、DateTime、builtins、modules、REPL 和 profiler 接入。GC root 规则无法由模块边界和类型接口表达，正是 HHY-GC-001/002 易重复出现的背景因素。

建议按稳定职责逐步拆成 `value/env/eval/stream/io/process/http/codec/runtime_owner`，先移动不改变行为的纯函数，再建立 value-buffer 所有权 API；不建议一次性重写。

### HHY-PROF-001：JSON profile 对低样本结果缺少可靠性标记

**Severity:** P3
**Category:** Profiler / Developer experience

**Location:** `src/profiler.c:219-236` 与 `247-273`

文本输出在样本少于 10 时提示“不稳定”，JSON 只输出 `cpu_samples`。本次 2–3 ms 工作负载得到 0 或 1 个样本；消费方很容易把单样本的 100% 当成稳定热点。

建议 JSON 增加 `cpu_sample_period_us`、`cpu_data_quality: insufficient|usable` 或 `warnings`，并为 0/1/9/10 samples 增加快照测试。

### HHY-DOC-001：历史性能报告与当前代码状态存在断言偏差

**Severity:** P3
**Category:** Release evidence / Documentation

`performance-analysis/2026-08-27-runtime-hardening/02-test-report.md` 声称“所有跨 GC 分配点存活的 Value 临时数组改为 GC 托管内存”，但当前 `stream_collect`、JSON、CSV、distinct/debounce 仍使用普通 heap。发布证据应绑定 commit、列出精确覆盖函数，并避免“所有”这类无法自动验证的断言。

建议增加静态审查规则：禁止 `Value *`、`char **`（持有 GC String 时）通过 native allocator 跨越托管分配点；报告由 CI 记录 commit 与命令结果。

## Lexer / Parser / Checker Review

- Token 保存源码切片、行、列；源码先做 UTF-8 校验；CRLF、shebang、注释、字符串转义、regex/division 上下文消歧均有明确实现。
- Parser 使用优先级表达式解析并包含 panic recovery；`multiple-errors.hhy` 实测给出两个独立错误和正确的源码位置。
- Resolver 采用 static slot → inline cache → name lookup 降级；动态 import/跨函数捕获保留慢路径，设计方向合理。
- 当前没有 bytecode compiler，所谓 compiler correctness 应落在 AST construction、checker 与 resolver 的一致性上。
- 未发现已确认的运算符优先级、EOF 或 closure parser 错误；现有测试对 invalid string escape、unterminated block、module cycle、overflow 等覆盖较好。

## Runtime Object / Closure / Stream Review

- `Value` 覆盖 Null、Bool、Int、Float、String、Regex、List、Range、Map、Function、Stream、Path、units、Result、File/Directory/Event、Process/CommandResult、HTTP、BytesBuffer、Error、DateTime。
- String 使用显式 byte length；`length`/index 以 Unicode code point 计，NUL 测试存在；Path 与 String 类型保持区分。
- 数字 equality/hash 对 Int/Float、0/-0 做统一处理，`group_by` 冲突后再调用 equality，方向正确。
- 闭包通过 Env 保活；`frame-slots-escape.hhy` 实测输出 `11, 12, 101, [8, 9, 10]`，覆盖多个 factory、captured mutation 和返回后惰性 Stream。
- Stream 为 pull-based、single-consumption；take/skip/map/where/flat_map/on_error/parallel 等共享 `stream_next`，资源由 `stream_close` 汇聚释放。
- barrier 对已知无界 every/watch 做拒绝，但静态有界性只沿 Stream kind 递归推断；这是保守可接受的 v1.x 限制。

## Error and CLI Review

实测：

| 场景 | Exit | stdout/stderr |
| --- | ---: | --- |
| `hhy --help` | 0 | stdout，完整 usage |
| `hhy run` 缺参数 | 3 | stderr，明确缺 source |
| 不存在脚本 | 4 | stderr，明确 cannot open |
| invalid syntax | 2 | stderr，文件/行/列/源码片段 |
| runtime limit/error | 由测试覆盖 | 结构化 Error + 稳定 exit mapping |

整体符合 Unix CLI 习惯。Runtime error 输出包含文件、行、列及结构化 Error；当前不提供传统 VM call stack，这是 AST evaluator 的可观测性短板，但没有作为 v1.0 冻结 contract 明确承诺。

## Profiler and Performance Baseline

环境：macOS arm64，本地 Release build；以下为单次 profile，仅用于本 commit 诊断，不作为跨机器排名。

| Workload | Wall | CPU samples | Allocations | Allocated | Heap peak |
| --- | ---: | ---: | ---: | ---: | ---: |
| Fibonacci(30) | 256.6 ms | 160 | 4,038,978 | 215.4 MB | 2.18 MB |
| collections/reduce | 2.50 ms | 0 | 211 | 13.1 KB | 77.8 KB |
| JSON pretty encode | 2.95 ms | 0 | 145 | 8.2 KB | 65.5 KB |
| CSV Flow | 2.54 ms | 1 | 208 | 13.2 KB | 73.7 KB |
| closure/frame escape | 2.40 ms | 1 | 157 | 11.3 KB | 77.8 KB |

Fibonacci 仍执行 2,692,537 次函数调用并产生约 1.50 allocations/call、约 80 bytes/call。Heap 峰值低而累计分配高，说明 GC 能回收但 AST 调用路径仍有显著 churn。短程序的 0–1 样本数据不可用于热点百分比判断。

需求中 integer loop、float、List、Map、String、Range、Pipe、Stream、JSON、File 均有现有测试覆盖，但仓库缺少统一、可重复、输出机器可读统计的 benchmark harness；当前 `profile` 示例更像功能演示。建议建立固定数据规模、预热、重复次数、中位数和环境元数据的 benchmark suite。

## Build / Release / Cross-platform

- Makefile 使用 C11、`-Wall -Wextra -Wpedantic -Werror -O2`，Debug 使用 ASan/UBSan；依赖文件通过 `-MMD -MP` 跟踪。
- CI 在 Linux x86_64、Linux arm64、macOS arm64 原生 runner 执行 release、sanitizer、fuzz、docs 和 dist 验证；Linux x86_64 的 BDWGC/ASan 已知限制有文档说明并改用 UBSan 完整测试。
- release workflow 校验 tag/version、架构、归档 checksum、解压后运行和扩展安装，并生成 attestation。
- 平台相关 watch、rename、process snapshot 均有条件实现。Windows 明确不在 v1.0 支持范围。
- 本次本地 `make test` 与 `make test-debug` 均在 process snapshot 断言处失败；失败根因是审查 sandbox 禁止 `/bin/ps`，不是 sanitizer 报告。此前的 lexer/parser/runtime 用例均执行到该点，未见 ASan/UBSan 输出。

## Test Coverage Assessment

现有规模：65 个 valid、26 个 invalid/invalid-runtime、6 个 acceptance、10 个 examples，另有 docs、contracts、extension protocol、practical projects 与 fuzz harness。覆盖面好，尤其包括递归限制、整数溢出、Unicode surrogate、NUL String、Stream early close、parallel cancel/error、frame escape、Map hash semantics 和原子 unwind。

关键缺口：

1. 可在精确分配点触发的 deterministic GC stress。
2. 所有 native `Value` buffer 的 root-lifetime 回归矩阵。
3. 大规模 distinct 与宽 Map/JSON 的复杂度门槛。
4. process capability denied/empty/error 的可诊断测试。
5. profiler JSON 的低样本质量字段测试。
6. 统一 benchmark harness 与历史回归阈值。

## Language Semantics Assessment

HHY 的主要语义选择相互协调：严格 Bool、无危险隐式 String 转换、Int64 溢出报错、Int/Float 数值相等、String code-point length、Path/String 类型区分、不可变集合、显式 mutable binding、Stream single-use、普通错误 fail-fast、parallel frozen snapshot。已知限制文档也明确区分 HTTP 有界 body、barrier 物化、process-based parallel 与非稳定 ABI。

本次没有确认新的语言设计矛盾。主要风险来自实现未完整满足已冻结语义，而非语义本身不合理。

## Recommended Remediation Order

1. 修复 HHY-GC-001/002；引入 scanned/native buffer 类型化 API。
2. 加入 deterministic GC stress，覆盖 collect、JSON、CSV、distinct、debounce、sort/group。
3. 复用 `hash_scalar` 将 distinct 改为平均 O(n)。
4. 为 Map/record 增加保序 hash index，先优化 lookup，再处理构造重复检测。
5. 拆分 process snapshot 测试的 capability 与 serialization 责任。
6. 拆分 `runtime.c`，优先抽出 value buffer/codec/stream ownership。
7. 为 profiler JSON 增加数据质量元信息，建立统一 benchmark harness。

## Release Gate

建议 v1.1.2 release candidate 的最低门槛：

- GC stress 下 collect/JSON/CSV/distinct/debounce 连续运行无错误。
- Release 与各平台 sanitizer/UBSan 矩阵通过。
- `make test` 在有 process capability 的原生 CI 全绿；受限环境明确 skip，而不是 Null 断言失败。
- 10k/20k/40k distinct 不再呈平方增长。
- runtime-hardening 报告更新为与当前 commit 一致的证据。

达到以上条件后，Production readiness 可提升至约 82–86/100；字节码并不是 v1.1 发布的必要条件，当前更重要的是先封闭 GC ownership contract。
