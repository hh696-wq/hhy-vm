# HHY VM Technical Review Remediation Log

日期：2026-08-30
修复基线：`6a56793`（工作区包含本日志所列未提交修复）
对应审计：`HHY-VM-TECHNICAL-REVIEW.md`
设计约束：保持 HHY v1 冻结语义；通用 HTTP 兼容默认值不变；不以应用层补丁代替 Runtime 对象模型修复。

## 修复结论

| 审计项 | 状态 | 处理 |
| --- | --- | --- |
| HHY-CRAWLER-SEC-001 | 已修复 | Crawler 入口归一化配置；只有显式 Bool `true` 允许私网，缺失、Null、错误类型和拼错字段均 fail-closed |
| HHY-PERF-003 | 已修复 | `Value` 改为持有 `MapStorage *`；Map 索引元数据不再扩大所有 Value；增加布局静态断言 |
| HHY-CRAWLER-PERF-001 | 风险已封顶，结构性方案待 RFC | 对并发、深度、页面、Frontier、单页链接和结果数设置保守硬上限；未擅自向冻结语言加入 mutable Set/List builder |
| HHY-TEST-002 | 已修复主要缺口 | 新增动态 String distinct GC stress，并将 debounce 纳入逐分配 GC 矩阵 |
| HHY-ARCH-002 | 独立重构项 | 本轮不把 6k 行 Runtime 拆分混入安全/布局修复；应单独移动代码、单独审查，不改变行为 |
| HHY-EVIDENCE-001 | 待当前提交远端 CI | 本机 macOS arm64 Release、ASan+UBSan 和项目验收已记录；未伪造 Linux/远端成功证据 |

## 1. Crawler 安全配置边界

修改：

- `practical-projects/my-crawler/lib/engine.hhy`
- `practical-projects/my-crawler/self-test.sh`

`crawl(raw_config)` 在任何 URL 处理前创建归一化配置：

```hhy
allow_private_networks: raw_config.allow_private_networks == true
```

因此通用 `http.get` 的兼容行为保持不变，而官方 Crawler 面向不可信 URL 时默认拒绝 loopback、RFC1918、link-local 等连接地址。fixture 必须显式设置 `true`。

同时为应用级可放大参数设置硬上限：

| 参数 | 默认回退 | 硬上限 |
| --- | ---: | ---: |
| `parallelism` | 4 | 32 |
| `max_depth` | 3 | 32 |
| `max_pages` | 50 | 10,000 |
| `max_frontier` | 100 | 20,000 |
| `max_links_per_page` | 200 | 5,000 |
| `max_results` | 100 | 10,000 |

非 Int、零值和负值使用安全默认值；超过上限时收敛到硬上限。没有修改 VM 的 `RuntimeLimits`，两层边界职责保持独立。

新增验收覆盖：

1. 显式 `true` 的本机 fixture 成功抓取 3/3 页面。
2. 直接 HTTP 显式 `false` 拒绝 loopback。
3. Crawler 配置省略 `allow_private_networks` 时仍拒绝 loopback，失败记录包含私网拒绝诊断。

## 2. Value / Map 对象模型

修改：`src/runtime.c`

旧表示把 `slots`、`slot_count`、`index_magic` 连同 Map 数组直接嵌入 `Value` union，使每个 Int、Bool、String 和调用帧 Value 都承担 Map 大小。

新表示：

```c
typedef struct MapStorage MapStorage;

struct Value {
    ValueKind kind;
    size_t string_length;
    union {
        /* scalars */
        MapStorage *map;
        /* other values */
    } as;
};

_Static_assert(sizeof(Value) <= 56, "...");
```

实现细节：

- `MapStorage` 使用 GC scanned allocation，继续保活 keys、values 和索引指针。
- slots 仍使用 atomic allocation；小 Map 仍不创建哈希索引。
- Map iteration、duplicate key、数值 equality、二进制 worker 序列化协议均保持原语义。
- 内存上限异常路径不能再次分配，因此 `Runtime` 内保留预分配 `emergency_error_map`，避免在 OOM 处理期间递归进入分配器。

Fibonacci(30) 对比：

| 指标 | 审计前回归 | 修复后 | 结果 |
| --- | ---: | ---: | --- |
| Function calls | 2,692,537 | 2,692,537 | 不变 |
| Allocations | 4,038,971 | 4,038,975 | 基本不变（+4） |
| Allocated bytes | 258,497,607 B | 215,415,541 B | -16.67%，恢复旧基线 |
| Heap peak | 2,175,032 B | 2,175,016 B | 不退化 |
| Wall（单次，仅诊断） | 272.6 ms | 270.1 ms | 不退化 |

## 3. GC stress

新增 `tests/valid/gc-distinct-dynamic.hhy`，通过 `encode_json({ value })` 生成 2,000 个运行期 String，再执行 `distinct`。普通模式和 `HHY_GC_STRESS=1` 均输出 `2000`。

`tests/run.sh` 的 Release GC 矩阵现包含：

- advanced flow
- JSON flow
- CSV flow
- dynamic String distinct
- debounce

debounce 普通模式和逐分配 GC 均输出 `[1, 2, 3]`。

## 4. 验证记录

```text
make test
all lexer, parser and runtime tests passed

make test-debug
all lexer, parser and runtime tests passed
ASan + UBSan: passed

./practical-projects/my-crawler/self-test.sh
Pages 3 / 3 Records 3 Failures 0
default-deny loopback test: passed

./practical-projects/sitegraph-auditor/self-test.sh
healthy: Pages 4 / 4, Edges 5, Findings 0
risky: Pages 1 / 2, Edges 2, Findings 3

git diff --check
passed
```

测试环境：macOS arm64。HTTP 验收仅绑定临时 `127.0.0.1` 端口。

## 5. 明确未混入本补丁的工作

### Ordered Set / List builder

公开 `set()`、`set_add()`、`set_contains()` 会新增 Value kind、hashability contract、并行 worker 序列化规则和可变性语义。这不是可以藏在 Crawler 内的普通脚本优化。当前以硬上限封闭资源风险；结构性 O(n) Crawler 应先完成语言 RFC、contract、invalid tests 和跨 worker 设计，再独立实现。

### Runtime 模块拆分

文件拆分本身不修复语义缺陷，并会显著放大本轮指针表示变更的 review diff。建议在此补丁稳定后，以“只移动实现、零行为变化”的独立提交拆出 value/map、stream、http/url。

### 跨平台证据

本日志只声明已实际执行的本机结果。修复提交推送后，需补充当前 commit 的 GitHub Actions run URL、macOS arm64 / Linux arm64 / Linux x86_64 job conclusion 与 artifact digest，才能关闭 HHY-EVIDENCE-001。

## Release 判断

P1 发布阻断项已关闭；Value 全局分配回归已关闭；GC stress 主要缺口已固化。允许进入提交前 review。Crawler 的大规模线性状态结构、Runtime 文件拆分和当前提交跨平台证据保留为明确的后续工程项，不应在没有 RFC 或远端证据时标记为完成。
