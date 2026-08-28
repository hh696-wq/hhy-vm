# HHY v1.1.1 Runtime 修复测试报告

日期：2026-08-27
对应诊断：[01-diagnosis.md](./01-diagnosis.md)
最终结果：通过

## 实施范围

### CPU 循环取消安全点

- 为普通 `while` 和列表/Stream `for` 增加低频执行安全点。
- 每 1024 次迭代检查一次 `SIGINT`、Runtime 取消状态和 `max_runtime`。
- 新增纯 CPU 无限循环测试，确认 `max_runtime=20ms` 返回状态 5 和 `TimeoutError`。

### Managed Heap 检查降频

- 删除每次 `GC_malloc` 前后重复读取 GC 全局状态的路径。
- 改为溢出安全的 256 KiB 分段预算；接近边界时读取 managed heap，超过限制时执行一次 GC 后复验。
- Heap profile 使用分段观测值估算区间峰值，并在结束前强制 GC 后记录最终值。
- `max_memory=64kib` 和原子文件写入回滚测试通过，越界返回 `HHY_MEMORY_LIMIT`，没有遗留临时文件。

### Profiler 热路径降扰动

- 新增长度感知的 `hhy_profiler_enter_n()`。
- 函数名称直接引用 AST Token，仅首次建立热点条目时复制。
- 已存在热点不再切换 `SIGPROF` 信号掩码。
- CPU-only profile 不再读取 Heap 状态；Heap 分配记账不再逐对象调用 `GC_get_memory_use()`。

### 并行 worker 有界退出

- 清理时先发送 `SIGTERM`，以 10 ms 间隔等待最多 500 ms。
- 未退出的 worker 升级为 `SIGKILL`，随后同步 `waitpid` 回收。
- 成功、错误、取消和 Stream 提前关闭共用相同清理逻辑。

### `sort_by` 复杂度优化

- 将稳定插入排序替换为稳定、自底向上的归并排序。
- 最坏时间复杂度由 `O(n²)` 降为 `O(n log n)`。
- 相同键继续保持输入顺序，并保留原有数值、String、Path、单位值和 DateTime 比较语义。
- 每轮归并后执行取消检查。

`group_by` 的哈希优化本轮没有贸然加入。它仍是已确认的后续热点；需要先确定 Float、`-0` 和字符串哈希与当前相等性语义的一致性。

## 性能对比

工作负载：`examples/09-profile-algorithms.hhy fibonacci 22`。

| 模式 | 修改前 | 修改后 | 变化 |
| --- | ---: | ---: | ---: |
| 普通运行 | 约 0.77 s | 约 0.02–0.03 s | 约 25–38 倍加速 |
| CPU profile | 约 3.27 s | 约 0.02–0.03 s | 约 109–163 倍加速 |
| CPU+Heap profile | 1.895 s | 0.020 s | 约 94 倍加速 |
| Profile 相对普通运行扰动 | 约 4.2 倍 | 接近 1 倍 | 显著降低 |

最终完整 profile 摘要：

```json
{
  "wall_seconds": 0.020319,
  "cpu_seconds": 0.018825,
  "cpu_samples": 16,
  "heap_peak_bytes": 2166950,
  "heap_current_bytes": 16384,
  "allocated_bytes": 42133719,
  "allocations": 687910
}
```

累计分配仍约为 40 MiB，说明调用帧、Binding 和标识符仍有结构性优化空间；本轮先移除了逐分配 GC 查询这一主要放大器。

## 自动化测试

### Release

```sh
make test
```

结果：`all lexer, parser and runtime tests passed`。

### ASan / UBSan

```sh
make test-debug
```

结果：`all lexer, parser and runtime tests passed`，没有 Sanitizer 报告。

### 非稳定状态 139 复验

第一次 Release 全量测试曾在 `tests/acceptance/03-process-system.hhy` 附近返回一次状态 139。复验如下：

- 单独重复运行该用例 100 次：全部通过。
- 再次运行 Release 全量测试：通过。
- 排序实现完成后再次运行 Release 全量测试：通过。
- 修改前后两轮 ASan/UBSan 全量测试：均通过。

目前无法稳定复现，也没有 Sanitizer 证据。报告保留该记录；后续 CI 应持续观察 process snapshot 用例。

## Multi-API Data Collector

执行：

```sh
sh practical-projects/multi-api-data-collector/self-test.sh
```

覆盖 6 个并行页面、12 条输入记录、9 条去重记录、连续运行两次、增量合并、CSV/report/failures 输出，以及 fixture 与 worker 退出。

结果：

```text
verified: 6 pages, 12 incoming records, 9 unique records
HHY Multi-API Data Collector self-test passed
real 1.32
user 0.08
sys 0.06
```

修改前约 1.39 s。该 fixture 主要受固定 `sleep(250ms)` 影响，因此重点验收项是输出一致和所有 worker 正常退出。

## 残留进程

全部测试完成后检查结果：

- 残留 `hhy`：0
- 残留 `test-server.py`：0
- 残留扩展进程：0

## 已知后续工作

1. 为 `group_by` 增加保持首次出现顺序的哈希索引。
2. 缓存 AST 标识符和参数名，减少 `token_text()` 分配。
3. 为小参数函数提供栈上参数槽或轻量调用帧。
4. 在 profile 中区分 managed heap、主进程 RSS 和 worker RSS。
5. 为扩展协议阻塞读取增加 deadline，并对扩展进程采用 TERM/KILL 有界升级。
6. 将 process snapshot 100 次重复测试加入持续压力测试。

## 最终判断

本轮修复解决了纯 CPU 循环不可取消问题，显著降低了 GC 限制检查和 Profiler 对算法的扰动，强化了并行 worker 回收，并将 `sort_by` 改为稳定的 `O(n log n)`。

Release、ASan/UBSan、Collector、CPU 超时、内存回滚和残留进程检查全部通过，可以进入代码审查与提交阶段。

## Go / PHP / Python 同算法对比

测试环境：macOS 26.6.2、Apple arm64；Go 1.27.0、PHP 8.5.9 CLI、Python 3.14.7、HHY 1.1.1。

四种语言均使用没有缓存和循环改写的朴素递归 Fibonacci(30)，结果均为 `832040`。Go 预先编译，编译时间不计入运行时间；四种命令都计入进程启动时间。每种语言预热 2 次，再以固定随机顺序交错执行 10 次，使用墙钟时间中位数。

| 语言 | 中位数 | 平均值 | 最小值 | 最大值 | 相对 Go | 相对 PHP |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Go | 4.21 ms | 4.19 ms | 3.70 ms | 4.71 ms | 1.0× | 0.07× |
| PHP | 57.34 ms | 58.23 ms | 56.48 ms | 63.84 ms | 13.6× | 1.0× |
| Python | 58.93 ms | 59.07 ms | 58.32 ms | 61.26 ms | 14.0× | 1.03× |
| HHY | 681.27 ms | 692.37 ms | 655.93 ms | 741.18 ms | 161.9× | 11.9× |

结论：本轮已经移除了 GC 查询和 Profiler 的异常放大器，但 HHY 在函数调用密集型递归算法上仍约比 PHP/Python 慢 11.6–11.9 倍。Fibonacci(30) 的 HHY profile 显示 2,692,537 次函数调用产生 32,310,598 次分配，累计分配约 1.98 GB；主要剩余瓶颈是每次调用创建 Env、Binding、参数数组和标识符副本，而不是 GC 峰值（峰值约 2.18 MB）。

下一步优化应直接针对调用帧：缓存 Token 文本、为少参数调用使用栈上参数槽、以连续 frame 替代每次调用的 Env/Binding GC 对象。只有完成这一层，HHY 才可能明显缩小与 PHP/Python 的差距。

## 第二阶段优化结果

### Fibonacci(30) 调用与分配

优化后 profile：

| 指标 | 第一阶段 | 第二阶段 | 变化 |
| --- | ---: | ---: | ---: |
| 墙钟时间（单次 profile） | 约 0.697 s | 0.285 s | -59.1% |
| 函数调用 | 2,692,537 | 2,692,537 | 不变 |
| 分配次数 | 32,310,598 | 4,038,947 | -87.5% |
| 累计分配 | 约 1.98 GB | 328.7 MiB | 约 -83.4% |
| Heap 峰值 | 约 2.18 MiB | 2.1 MiB | 基本不变 |

调用次数完全一致，说明结果没有通过缓存 Fibonacci 或改变算法获得；收益来自减少调用帧、参数与标识符的短命分配。

### Go / PHP / Python 复测

仍使用朴素递归 Fibonacci(30)，每种语言预热 2 次，以固定随机顺序交错运行 10 次并取中位数：

| 语言 | 中位数 | 最小值 | 最大值 | 相对 PHP |
| --- | ---: | ---: | ---: | ---: |
| Go | 4.80 ms | 3.84 ms | 5.20 ms | 0.08× |
| PHP | 60.78 ms | 57.43 ms | 68.17 ms | 1.00× |
| Python | 61.88 ms | 59.52 ms | 74.74 ms | 1.02× |
| HHY | 257.59 ms | 243.34 ms | 264.29 ms | 4.24× |

HHY 中位数由 681.27 ms 降至 257.59 ms，提升约 2.64 倍；与 PHP 的差距由约 11.9 倍缩小到约 4.24 倍。

### `group_by` 语义与规模

- `1` 与 `1.0` 合并；`0`、`0.0` 与 `-0.0` 合并。
- 内容相同的 String 与 Path 保持不同分组。
- Null、Bool 重复键正确合并，首次出现顺序不变。
- 20,000 个唯一键测试输出 20,000 个组。
- 该规模 profile：0.008 s，Heap 峰值 6.5 MiB，累计分配 8.3 MiB。

### 回归结果

- `make test`：通过。
- `make test-debug`：通过，无 ASan/UBSan 报告。
- Multi-API Data Collector self-test：通过；6 页、12 条输入、9 条唯一记录，连续两轮结果一致。
- `git diff --check`：通过。

### 剩余热点

调用密集场景仍约比 PHP/Python 慢 4.2 倍。下一阶段可评估真正的连续轻量 Frame、AST 标识符预解析/驻留，以及避免返回值和算术表达式中的剩余托管分配。任何进一步缓存都必须保持闭包捕获、模块生命周期与动态 builtin 查找语义。

## 第三阶段：Slot Frame 测试报告

朴素递归 Fibonacci(30)，预热 3 次、固定随机顺序交错执行 20 次：

| 语言 | 中位数 | 平均值 | 最小值 | 最大值 | 相对 PHP |
| --- | ---: | ---: | ---: | ---: | ---: |
| Go | 4.43 ms | 4.48 ms | 3.73 ms | 6.53 ms | 0.08× |
| PHP | 57.43 ms | 57.40 ms | 56.27 ms | 58.45 ms | 1.00× |
| Python | 59.08 ms | 59.30 ms | 58.42 ms | 60.41 ms | 1.03× |
| HHY | 200.79 ms | 202.02 ms | 198.19 ms | 221.30 ms | 3.50× |

相对第二阶段的 257.59 ms，本阶段再提升约 1.28 倍；相对最初 681.27 ms，累计提升约 3.39 倍。与 PHP 的差距从约 11.9 倍缩小到约 3.50 倍。

Profile 中函数调用仍为 2,692,537 次，算法与结果没有改变：

| 指标 | 第二阶段 | Slot Frame | 变化 |
| --- | ---: | ---: | ---: |
| Profile 墙钟 | 0.285 s | 0.231 s | -18.9% |
| 分配次数 | 4,038,947 | 1,346,440 | -66.7% |
| 累计分配 | 328.7 MiB | 61.6 MiB | -81.3% |
| Heap 峰值 | 2.1 MiB | 2.1 MiB | 基本不变 |

新增 `frame-slots-escape.hhy` 验证两个工厂调用返回的嵌套函数拥有独立 captured mutable slot，外层返回后仍可连续修改捕获变量；同时验证函数返回惰性 Stream 后再执行 `collect`，仍能读取已返回调用帧中的局部变量。实际输出为 `11`、`12`、`101` 和 `[8, 9, 10]`。

完整验收：Release 全量测试通过；ASan/UBSan 全量测试通过且没有 Sanitizer 报告；Multi-API Data Collector 的 6 页、12 条输入、9 条唯一记录连续两轮自测通过；`git diff --check` 通过。

Resolver、Static Slot、Inline Cache 和逃逸安全 Frame Pool 是通用调用路径，没有针对 Fibonacci 函数名或参数写分支。HHY 仍约比 PHP 慢 3.5 倍，说明 AST 递归分派、通用 Value 运算和 C 调用层级开始成为更明显的剩余成本。下一步应先细分整数运算与 AST dispatch 占比，再决定后续优化，而不是现在直接上字节码。

## Release CI GC 根生命周期修复

GitHub Actions 的三种 Release 配置都曾以状态 139 退出，而 Debug + ASan/UBSan 正常。普通日志只显示崩溃前较早出现的 `shell.hhy` 警告；临时启用逐命令跟踪后，准确定位到 `group-hash-scale.hhy`。

根因是排序/分组物化阶段用普通 `malloc` 数组暂存 `Value`。这些数组不会被 Boehm GC 扫描；创建各分组 List 时触发 GC 后，先前创建的 List 及 String、Path、Map 等值可能失去根并被回收。commit `8bc7df9` 当时修复了本报告覆盖到的排序/分组与调用参数路径；这里原先使用“所有跨 GC 分配点”的表述超出了该次审查证据。后续全 Runtime 扫描在 `2026-08-27-technical-review` 中又发现 collect、JSON、CSV、distinct/debounce 同类路径，现已由后续修复统一迁移到具名 GC-scanned buffer API，并加入静态规则与确定性 GC 压力回归。纯 `size_t` 哈希槽、计数和索引仍使用普通内存并明确释放。轻量 Frame、slot、标识符缓存与预分配容量等通用优化继续保留。

本地验收：

- Release 全量测试通过。
- Debug + ASan/UBSan 全量测试通过，无 Sanitizer 报告。
- `shell.hhy` 与逻辑记录并行序列化交替压力执行 50 轮通过。
- `group-hash-scale.hhy` 与混合类型哈希语义测试交替压力执行 30 轮通过。
- 进程快照排序验收压力执行 50 轮通过；公共 `map_with_entries` 会先复制并托管输入 Value，再分配 key 元数据，避免大 Record 集合构造期间丢失新建字符串的 GC 根。

Linux 的第二处崩溃通过 gdb 确认为 `runtime_release → stream_close → fclose`。根因是顶层 `Runtime` 使用普通 `calloc`，Boehm 不会扫描其中的 `streams/modules/free_call_frames` 所有权链；已经消费完、只由该链保留的 Stream 可能在清理前被回收。`Runtime` 现改为 `GC_malloc_uncollectable`：控制块仍由运行时在 teardown 后显式 `GC_free`，但其内部 GC 对象引用在整个运行期均作为可扫描根。进程快照排序验收扩大到 100 轮通过。
- Multi-API Data Collector 自测通过：6 页、12 条输入、9 条唯一记录，连续两轮一致。
- Fibonacci(20) 输出 6765，profile 记录 21,891 次函数调用；未增加算法特判。
