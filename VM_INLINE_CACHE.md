# v1.6.2：条件式 Inline Cache 评估

本阶段交付本地画像、可关闭的单态 Map slot 实验、差分/资源测试和可复现实验；不表示发布版本或通过跨平台默认启用准入。不能把局部测量推广为“缓存无收益”。

## 已有路径与评估范围

- 名字解析已有 resolver slot/depth；AST 节点还可保留带名称校验的缓存。Bytecode 的临时 site 重建不保留这类后写缓存，画像可见重复 search；本阶段不把它误记为 cache hit。运行时始终从当前环境读取 Binding 值。新增持久名字缓存需要独立处理遮蔽与失效，并证明真实负载收益。
- builtin identifier 仍经过既有解析；`builtin_resolutions` 统计通用 identifier 路径成功产生 builtin callable 的次数（不含特殊 `processes` 值）。
- call target 已保存 AST node 或 Bytecode chunk/instruction；画像观察代码目标稳定性，不缓存捕获值，不绕过参数、effect、contract、递归或取消检查。
- Map 少于 8 项走线性查找，较大 Map 已有哈希索引。实验比较单态 slot 提示与这两种现有路径。
- 分别测量 binding、通用 call_value 和解释器 MEMBER/字符串 INDEX。融合 Stream/专用内核、内部 C helper 的 Map 读取和专用快速 closure 调用不包含在这些计数中。操作频次占比不等于 CPU 时间占比。

## 开关、边界与观测

`HHY_PROFILE_LOOKUPS=1` 仅在 profile 命令开启反馈；普通 run 不为此分配。`HHY_MAP_INLINE_CACHE=1` 开启 Map 实验，可独立运行。两者默认关闭，关闭时没有反馈表分配。

每个 Runtime/Context 最多三个域各 64 个源码位置，线性探测解决哈希碰撞，域满时不替换旧条目、不扩容，回通用路径并计 dropped。位置身份由当前 source 身份、line、column 共同组成，不能使用 Bytecode 的 interned token 字符串地址代表源码位置。
表用 atomic managed allocator 分配并计入内存配额；内容仅有计数、数值身份、slot、复制的路径字节，不持有可解引用的 Map/Value/Closure/Binding 指针。退出或 managed quota 失败沿用原有 Context 资源规则。

`lookup_profile` JSON schema v1 输出：

- binding 的 resolved/cache_hit/search/absent，以及 builtin_resolutions。
- 每个域的 observations/dropped；每个位置的 observations/stable/transitions/distinct_capped/megamorphic。
- Map 的 guard_hit/guard_miss、cold、slot_or_key_changed、key_missing、megamorphic_fallback、cached_reads/generic_reads。
- reserved_bytes 与 sites_per_kind_limit；源码位置以 source_bytes/line/column 输出。source_bytes 是最多 127 个路径字节的 Latin-1 JSON 映射，非 ASCII 路径可用 `.encode('latin1').decode('utf8')` 还原；截断用 source_truncated 标明，仅用于显示，不参与身份比较。

call target 的反馈身份是代码目标，builtin 使用名字摘要；不等于 callable 环境或类型 shape 的证明，摘要碰撞与地址复用可能降低画像精度，不参与任何执行快路径。distinct_capped=5 表示超过四个目标，不能解释成精确计数。

## 单态 Map 实验

只保存上次成功查找的 slot，不保存 Map 或 Value。每次命中都校验当前 Map 的 count、该 slot 的 key length 和完整 key bytes，再读取当前 values[slot]。
因此换 Map、字段换位、同键新值、动态键、嵌入 NUL 都不会返回旧值；失配时通用查找重新确定 slot。不存在依赖对象地址或 hash-only 比较的正确性条件。

四次 slot/key guard 失败，或观察到超过四种 slot/缺失结果，位置永久进入 megamorphic fallback，直到 Runtime/Context 结束。不构建 PIC，不无限增长。不缓存“键不存在”的结果。
关闭缓存但开启反馈时仍运行 guard 模拟并执行通用查找；guard_hit 是候选命中，只有 cached_reads 才表示实际快路径读取。冷启动和域溢出均走通用路径；域溢出另由 dropped 精确统计。

## 本地验收与复现

```sh
make test
HHY_MAP_INLINE_CACHE=1 HHY_PROFILE_LOOKUPS=1 make test
HHY_MAP_INLINE_CACHE=1 HHY_PROFILE_LOOKUPS=1 make test-debug
python3 tests/check-inline-cache.py build/hhy
python3 scripts/check-inline-cache-policy.py build/hhy
python3 scripts/evaluate-inline-cache.py --baseline-binary /path/to/baseline/hhy --baseline-commit COMMIT --output build/inline-cache.json
```

差分覆盖双引擎、两个独立开关、字段换位、缺失键、含 NUL 的键、代码目标切换、域溢出、Closure 与异常；保留原有 GC、宿主失败恢复、资源上限、取消和 profiler corpus。
性能脚本分开收集画像与无画像配对计时，包含合成调用/Map、JSON/Core 流水线及真实 Asset Governance 审计，比较旧基线/新关闭/新开启，并检查输出、退出状态与审计报告内容。
单个真实项目的 fixture 不等于生产规模；单个平台的稳定性不等于跨平台收益。局部实验结果不支持引入多态缓存或 hidden shape；保留现有对象模型，默认不启用任何新缓存。后续可用同一工具重新评估，不把“未准入”改写成“已证明无收益”。
