# HHY v1.1.1 Runtime 性能、内存与临界边界诊断

日期：2026-08-27
范围：HHY Runtime、`hhy profile`、Flow 集合操作、并行 worker、Multi-API Data Collector

## 结论

HHY 当前的性能问题不只是解释型语言的固定成本。Runtime 在函数调用、标识符读取、内存限制检查、Profiler 记账、集合排序与分组方面存在可消除的高频开销；普通计算循环缺少取消安全点，并行 worker 的退出也缺少有界升级策略。这些问题会放大 CPU 与内存占用，并可能让失控脚本持续占用单核。

## 诊断基线

在当前 `main` 的 v1.1.1 构建上运行：

```sh
./build/hhy run --limit max_runtime=10s \
  examples/09-profile-algorithms.hhy fibonacci 22

./build/hhy profile --cpu --format json \
  examples/09-profile-algorithms.hhy fibonacci 22
```

观测结果：

| 项目 | 结果 |
| --- | ---: |
| Fibonacci(22) 普通运行 | 约 0.77 s |
| Fibonacci(22) CPU profile | 约 3.27 s |
| CPU profile 时间膨胀 | 约 4.2 倍 |
| Fibonacci 调用次数 | 57,313 |
| 完整 profile 分配次数 | 745,223 |
| 完整 profile 累计分配 | 42,706,802 bytes |
| Collector fixture 连续运行两次 | 1.39 s，正常退出 |
| 诊断时残留 hhy 进程 | 0 |

## 问题与风险排序

### P0：计算循环缺少取消安全点

普通 `while` 的每轮执行没有调用 `runtime_check_cancel()`。纯计算死循环可能忽略 `Ctrl+C` 和 `RuntimeLimits.max_runtime`，持续占用单核 100%。默认 CLI 的 `max_runtime=0` 又不会提供自动兜底。

需要在 `while`、列表 `for`、函数调用和长集合内部循环设置低成本安全点，让超时和中断在确定的最大延迟内生效。

### P1：函数调用产生过多短命对象

函数调用会分配参数数组、调用环境、Binding 数组、参数名副本、标识符文本和表达式中间对象。Fibonacci(22) 平均每次函数调用约产生 13 次分配。

短期应消除 Profiler 路径上的动态名称复制和零/少参数调用的无意义数组分配；中期应缓存标识符、函数元数据并提供轻量调用帧。

### P1：每次托管分配都查询两次 GC 全局状态

`rt_alloc()` 与 `rt_alloc_atomic()` 在每次分配前后调用 `GC_get_memory_use()`。这使内存限制检查进入所有对象分配的热路径。Fibonacci 基线中几十万次分配会放大为上百万次 GC 状态读取。

应改成溢出安全的累计分配预算，只在达到检查步长、接近上限或显式边界时读取 GC 状态并触发回收。

### P1：Profiler 显著扰动被测程序

CPU profile 使 Fibonacci(22) 从约 0.77 s 增长到约 3.27 s。当前每次函数调用可能动态复制函数名、线性查找热点表并切换 `SIGPROF` 信号掩码；Heap profile 又会在每次分配读取 GC 内存。

Profiler 应缓存调用点到 entry ID 的映射，避免在热路径复制名字；CPU-only 模式不应产生 Heap 统计开销。报告应明确 profile 是采样与插桩数据，不等同于无插桩墙钟时间。

### P1：排序和分组最坏为平方复杂度

`sort_by` 使用插入排序，`group_by` 通过线性扫描寻找分组。两者在数据量增大时最坏均为 `O(n²)`，会影响算法测试与 Collector 的合并、去重、排序阶段。

排序应改为稳定的归并排序或等价 `O(n log n)` 实现；分组应使用具有相等性校验的哈希索引，并保留首次出现顺序。

### P1：并行 worker 退出没有完整的有界升级

Collector 的 `parallel(3)` 使用 `fork()` worker。主进程通过 10 ms 间隔轮询 `waitpid(WNOHANG)`；清理路径发送 `SIGTERM` 后可能无期限阻塞等待。如果 worker 卡在不可取消的系统调用或内部循环，父进程可能无法及时退出。

清理应采用“SIGTERM → 有界等待 → SIGKILL → 回收”的策略，并确保所有错误、取消和提前关闭路径执行相同逻辑。

### P2：`max_memory` 不是完整的进程 RSS 上限

当前限制主要依据 Boehm GC 管理堆，不能完整覆盖 curl、普通 `malloc/realloc`、Stream 辅助数组、解析器数据、临时文件缓冲和并行子进程内存。并行 worker 的实际总 RSS 也不会出现在父进程 Heap profile 中。

文档和报告需要区分 managed heap、进程 RSS 与子进程资源；后续可增加 `getrusage`/平台 RSS 观测和 worker 汇总。

## 修复顺序

1. 为执行循环和长操作增加低成本取消安全点。
2. 为并行 worker 实现有界终止和强制回收。
3. 将 GC 内存限制检查从每次分配降为分段检查。
4. 降低 Profiler 函数进入路径的动态分配和查找成本。
5. 将排序替换为稳定的 `O(n log n)` 算法；为分组加入哈希索引。
6. 增加超时、死循环、内存临界值、排序规模、分组规模、Collector 退出和残留进程测试。

## 验收标准

- 纯计算无限循环能够被 `max_runtime` 和 `SIGINT` 终止。
- 所有并行 worker 在成功、错误、超时和取消后都被回收。
- Fibonacci(22) 普通运行和 profile 的耗时明显下降，Profiler 扰动比例降低。
- 大集合排序不再表现为平方增长。
- 内存越界返回稳定的 `HHY_MEMORY_LIMIT`，不崩溃。
- `make test` 与 Collector fixture 自测通过。
- 测试后不存在残留 `hhy` 或 Collector fixture 进程。

## 第二阶段：调用帧、标识符缓存与 `group_by` 哈希

### 调用路径的主要浪费

Fibonacci(30) 会进入 HHY 函数 2,692,537 次。旧路径在每次调用中分别创建参数数组、Env、Binding 数组、参数名副本、函数体 Block Env，并在每次读取标识符前生成以 `\0` 结尾的字符串。因此即使 GC 峰值不高，也会累计产生 32,310,598 次分配和约 1.98 GB 分配流量。

本阶段采用以下兼容性优化：

1. 最多 4 个参数使用 C 栈上的参数槽，不再进入托管堆。
2. Env 与已知数量的 Binding 一次连续分配；容量不足时仍安全扩容。
3. Binding 保存名称指针和长度，函数参数、`let`、`for`、`catch` 等直接借用 AST Token 的稳定文本，不再复制名称。
4. 标识符查找改为长度加 `memcmp`，只有进入 builtin 的慢路径时才构造结尾为 `\0` 的名称。
5. 函数体 Block 直接使用调用 Env，避免每次调用额外创建只有父指针的 Block Env。
6. 128 字节以内的数值字面量使用栈缓冲区解析。

这些变更不引入永久的全局字符串表，也不改变闭包捕获和词法作用域规则；AST 的生命周期覆盖整个执行期，因此借用 Token 文本是安全的。

### `group_by` 的哈希一致性

`group_by` 从逐组线性扫描改为开放寻址哈希表，平均复杂度由 O(n²) 降为 O(n)。哈希值必须满足“相等的值一定具有相同哈希”：

- Int 与 Float 沿用当前跨类型数值相等语义，因此 `1` 与 `1.0` 使用相同数值哈希。
- `0`、`0.0`、`-0.0` 在哈希前统一为正零。
- String 与 Path 虽然内容可能相同，但类型不同，不共享分组。
- Bytes、Duration、Percent 保留类型标签，零值同样标准化。
- 冲突后必须再次调用现有 `equal_values`，哈希不能替代相等性判断。

分组编号仍按键首次出现顺序分配，因此输出顺序不变。实现采用两遍构建：第一遍统计每组大小，第二遍按精确容量填充，避免为每个新组都分配输入总长度而造成 O(n²) 内存。

## 第三阶段：Slot Resolver 与轻量调用帧

参考方案后，将执行路径明确为：

```text
Static Slot → Identifier Inline Cache → Name Lookup
```

新增独立 AST Resolve Pass，在执行前建立词法作用域，为同一函数内可以静态确定的参数和局部变量记录 Env 深度、Binding 槽位，以及函数或闭包所需的 Frame 槽位容量。

函数参数、函数体直接作用域的 `let`/`fn`、嵌套 Block、`for` 与 `catch` 均按实际运行时 Env 层级解析。跨函数边界的捕获变量、全局变量、builtin，以及含动态 import 的作用域不会强行槽位化，继续进入缓存或名称查找路径。Resolver 在主程序、运行时模块和 REPL chunk 解析完成后执行，不改变 AST 求值顺序，也不引入字节码。

函数调用根据 Resolver 算出的容量一次准备连续槽位。调用结束时，未逃逸 Frame 清空 Value 引用并放回复用链；创建嵌套函数、闭包或 Stream 时，当前 Env 及父链标为 escaped。escaped Frame 永不清空、永不复用，继续由 GC 管理。因此返回嵌套函数或惰性 Stream 后，其捕获变量仍然有效。

不能静态槽位化的标识符首次解析后缓存 Env 深度和 Binding 槽位。每次命中前仍校验名称；不同环境形状复用同一 AST 节点时，失配会回退并重新绑定。

本阶段扩大 `HhyNode` 后还发现旧 Makefile 未跟踪头文件依赖，增量构建可能混用不同结构体布局。编译规则已加入 `-MMD -MP` 并包含生成的 `.d` 文件，公共头文件变化会自动触发受影响对象重编。
