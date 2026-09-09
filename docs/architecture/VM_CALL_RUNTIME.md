# v1.6.1 调用、Closure 与异常展开

本阶段范围是本地 Runtime 实现、可回退实验、语义与资源失败验证，以及明确的表示策略。
本文件不表示发布了 v1.6.1 版本，也不表示实验已通过跨平台性能准入；VERSION 继续沿用现有发布版本。

## 交付与执行模型

| 交付 | 实现与约束 |
| --- | --- |
| 调用布局 | `call_plan` v1，包含参数、函数体入口及 frame capacity；Compiler 生成，Verifier 从结构重建核对，Runtime 可直接定位 |
| 紧凑调用记录 | scanned managed 数组存储最多 72 字节的 RuntimeCallFrame；保存调用 Env、closure/target/argument roots、source/contract/effect、调用深度、trace checkpoint 与 profiler checkpoint |
| 异常区域 | exception region v1 记录 PROGRAM/FN/CLOSURE 归属、保护区间与 handler；嵌套 catch 不会捕获自身 handler 重新抛出的错误 |
| 调用展开 | call unwind table v1：回收未逃逸 Env → 恢复 source/contract/effect → 恢复 depth/trace → 恢复 profiler → 清零 roots；整表先 verify 再执行 |
| 宿主展开 | 原有 boundary table 清理 Stream、登记文件/原生描述符/临时路径、extension scope 和执行状态；调用记录在宿主 longjmp 后也逐帧展开 |
| 帧池 | 默认 legacy；可选 9 桶实验，最多缓存 64 个 Env / 64 KiB GC 分配容量；不回收逃逸 Env |

机器栈仍由 C 调用/返回和已有 setjmp/longjmp 承载。RuntimeCallFrame 是可跨 longjmp 存活的逻辑调用记录，
不是 DWARF/SEH 或机器返回地址的替代品，也不把解释器改成 trampoline。
正常 C 返回、语言 Error、取消及宿主内存配额跳转均使用同一版本化动作表恢复登记的 Runtime 帧。
源位置继续由 RuntimeStackFrame 和 Bytecode source mapping 提供；逻辑返回后恢复原 caller 的 trace/profiler checkpoint。

调用数组按需增长，不给每个调用单独分配节点。调用者只保留索引，不能跨嵌套调用持有数组元素指针，
因为嵌套调用可能扩容。新数组获得 roots 后清空旧记录，退出帧全部清零，不保留失效 Env、target 或参数引用。
数组由现有 scanned allocator 分配，计入 managed-memory 配额；高水位容量在 Context 生命周期内保留。
因此该数组的 reserved bytes 与空闲 Env 缓存的 64 KiB 上限是两个不同概念。

## Closure / upvalue 复核决策

保留 `Function.closure -> Env`、共享词法环境及基于 slot/depth 的访问，不新增扁平 upvalue 数组。
多个闭包访问同一 Env/slot，修改通过共享环境可见；不是复制捕获值。
Env 扩容可移动 Binding 数组，但捕获保存的是 Env，slot 索引在新的数组中继续解释，不缓存旧 Binding 地址。

`env_mark_escaped` 沿 parent 链标记，函数/闭包和延迟 Stream 都在捕获时标记环境。
因此从调用返回的闭包、异常中传出的闭包，以及延迟执行的 Stream 不会被帧池清空或复用其环境。
这会保留整条相关词法链和未用 slot，属于保守的资源取舍；没有完整 capture 描述符和真实负载收益前，
不通过扁平化改变共享可变捕获、递归绑定、作用域深度或 Stream 生命周期。

验证包含两个独立 counter、同一 counter 的 read/write 别名、互递归、throw/catch 传出闭包、
函数返回后的 Stream，以及强制 GC 后继续访问捕获值。新的调用记录只补充 roots，不替代 Boehm 对 C 栈的保守扫描。

## 尾调用决策

不做尾调用消除，包括直接自递归和互递归。每次用户函数/闭包调用仍增加现有 call depth，
超过 max_recursion 返回既有 HHY_RECURSION_LIMIT；trace 仍遵守现有 128 帧上限。
一般错误、取消和返回值的源码位置/stack 与 AST oracle 逐字节比较。
内存紧急错误继续保留既有 allocation-free Error 格式（其 stack 为 null），不借优化改变可见错误数据。

## 开关与观测

| 环境变量 | 默认 | 实验值 |
| --- | --- | --- |
| HHY_BYTECODE_CALL_PLANS | 关闭 | `1` |
| HHY_BYTECODE_EXCEPTION_TABLES | 关闭 | `1` |
| HHY_CALL_FRAME_POOL | `legacy` | `bounded` |
| HHY_CALL_FRAME_UNWIND | 关闭 | `1` |

后三个开关在 Runtime/Context 初始化读取；调用布局在创建 callable target 时选择。
基础正确性修复（宿主参数转换保护、文件资源清理）始终启用。

反汇编和 compile metrics 显示布局表及字节；Profiler `call_layout`、`exception_layout`、`frame_pool` 分别观测对应实验。
`call_unwind` schema/table v1 提供 enabled、pushed、returned、errors、cancelled、resources、active、peak_active、reserved_bytes。
resources 统计宿主配额跳转后弹出的调用记录，不是发生了多少次 OOM；正常完成或失败输出时 active 必须为 0，
pushed 等于各类退出计数之和。参数转换若在建立首个调用记录之前失败，不会凭空产生一条 resource frame。

## 验收与性能边界

- `tests/runtime_unwind.c`：两张动作表的版本、数量、动作顺序和无副作用拒绝。
- `tests/check-call-unwind.py`：32 种引擎/开关组合，448 次 run/profile 差分及真实 SIGINT；内存、超时、递归错误和闭包语义。
- `tests/runtime_call_unwind.c`：同一 Context 连续失败与恢复，断言 depth/trace/contract/effect 和整个已退出调用记录存储清零。
- `tests/check-call-unwind-gc.py`：48 组 Release 强制 GC 捕获与 roots 回归。
- `make test-vm-call-policy`：机器可读策略、实际默认开关和尾递归限制检查。
- 既有 call-layout、exception-region、frame-pool、Embedding、文件失败、Web、GC 与取消 corpus 继续执行。
- `scripts/evaluate-call-unwind.py`：基线/关闭/开启的配对时间、编译指标、Profiler 和二进制摘要；不能单独作为跨平台准入。

单平台本地验收与默认启用是不同结论。保留所有实验默认关闭；有可观测的资源或结构收益也不能替代真实性能与跨平台证据。
本阶段不提前实施 v1.6.2 inline cache、v1.6.3 GC/scheduler 或 v1.7 HIR/MIR。
