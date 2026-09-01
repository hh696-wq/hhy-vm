# HHY v1.1.4 Technical Review Remediation Design

日期：2026-08-30
对应审计：`HHY-VM-TECHNICAL-REVIEW.md`

## 目标

按风险从高到低修复本轮 1 个 P1、2 个 P2、3 个 P3，不改变已冻结语言语义，不一次性重写 Runtime。

## Phase 1：Crawler 安全配置 fail-closed

在 `crawl()` 入口创建归一化配置，所有 fetch 只接收归一化结果：

```hhy
let normalized = put(config, "allow_private_networks", config.allow_private_networks == true)
```

这使 missing/Null/错误类型均落到 false；若希望错误类型直接拒绝，可在归一化前显式验证。fixture 继续明确写 true。

同时验证：

- seeds、allowed_domains、allowed_path_prefixes 为非空/合法 List<String>；
- parallelism、max_depth、max_pages、max_frontier、max_links_per_page、max_results 为正数且不超过项目硬上限；
- user_agent、selectors 为非空 String；
- 未知关键安全字段应报配置错误，避免 typo 静默降级。

验收测试：

1. 缺失 `allow_private_networks` 抓取 `127.0.0.1` → `HHY_HTTP_PRIVATE_NETWORK`。
2. 字段拼错 → 启动期 ConfigError。
3. 显式 false + 重定向到 loopback → 拒绝。
4. 显式 true + 本机 fixture → 通过。

## Phase 2：恢复紧凑 Value 布局

建议目标表示：

```c
typedef struct MapStorage MapStorage;
struct Value {
    ValueKind kind;
    size_t string_length;
    union {
        /* scalar members */
        MapStorage *map;
        /* other pointer-sized members */
    } as;
};
```

`MapStorage` 由 GC-scanned allocation 持有 count、keys、key_lengths、values、slots、slot_count。slots 可继续 atomic allocation，由 storage 指针保活。小 Map 不建索引。

迁移顺序：

1. 新增 accessor，先替代直接 `value.as.map.*` 访问。
2. 将所有 Map constructor 收敛到一个 builder。
3. 切换 Value 表示。
4. 全量 Release/Debug/GC stress。

验收门槛：

- `sizeof(Value)` 固化为目标上限并在 CI 静态断言；
- Fibonacci(30) allocations 不增加，allocated bytes 回到旧基线附近；
- 10k/100k 宽 Map 尾部 lookup 不退化；
- Map iteration 顺序、duplicate key、Int/Float equality contract 不变。

## Phase 3：Crawler 增量 Set 与 builder

优先提供最小、可审计的保序 Set API，而不是把 mutable Map 泛化到语言全部路径：

```text
set() -> Set
set_add(set, scalar) -> Bool   # true 表示首次加入
set_contains(set, scalar) -> Bool
```

Set 只接受与 `distinct` 相同的 hashable scalar，复用 `hash_scalar + equal_values`。Crawler 用 Set 保存 fingerprint；frontier/pages 使用受限 builder，结束后 freeze 为 List。

验收：1k/5k/10k 唯一链接规模倍增，wall 与比较次数近线性；首次发现顺序不变。

## Phase 4：测试与模块边界

- GC stress 新增 dynamic String distinct、Map/FileEvent debounce。
- 抽出 `runtime_value_map.c`、`runtime_stream.c`、`runtime_http_url.c`，只移动实现，不改变接口。
- CI 审计快照记录 commit、run URL、job result、artifact digest。

## 建议提交拆分

1. `fix(crawler): default private-network access to denied`
2. `test(crawler): cover missing config and redirect SSRF`
3. `refactor(runtime): move map storage behind a pointer`
4. `perf(runtime): add Value layout and Map lookup baselines`
5. `feat(runtime): add ordered scalar set builder`
6. `perf(crawler): use set and list builders for frontier state`
7. `test(gc): stress dynamic distinct and debounce state`
8. `refactor(runtime): split map stream and http modules`
