# HHY v1.7.0 已知限制

> 当前版本：`1.7.0`；本文描述 v1.x 的公开限制。

本文记录 v1.7.0 的公开限制。限制不是未实现功能的替代说法；凡属于
[`HHY_V1.md`](HHY_V1.md) 发布条件的能力仍必须实现和验证。

## 平台

- 正式目标仅为 macOS arm64、Linux arm64 和 Linux x86_64。
- Windows x86_64 提供经过 CI 验证的 MSYS2 POSIX 发行归档；它包含 Runtime、必要 DLL、sample 与 html 扩展，但不等同于原生 Win32 ABI，且暂不包含 database 扩展。macOS x86_64 和其他 Unix 仍不在正式支持矩阵中。
- MSYS2 不提供 `ITIMER_PROF`，因此该环境的 profiler 保留调用计数、总 CPU/墙钟时间和堆统计，但不提供信号式 CPU hotspot 采样。
- v1.3.5 起 `hhy run`、`hhy profile` 与脚本简写默认使用 Bytecode；AST 仍可通过 `--engine ast` 或 `HHY_ENGINE=ast` 显式回退。v1.3.10 实测 compile+verify 未达到缓存准入门槛，因此不提供持久 `.hhyc`、进程/磁盘 Bytecode 缓存、外部 Bytecode 加载器或公开 Bytecode ABI。
- File.created 依赖操作系统与文件系统；无法可靠取得时返回 Null，绝不使用 modified 伪造。
- watch 事件在 macOS/Linux 由 kqueue 或 inotify 归一化；其他 POSIX 环境使用文件状态轮询降级，递归目录监听能力有限。操作系统可能合并短时间内重复发生的底层事件。
- Unicode 大小写转换使用平台宽字符表；UTF-8 有效性和 code-point length 是确定的，但 v1.0 不承诺跨 Unicode 数据库版本完全一致的大小写映射，也不支持 grapheme-cluster 索引。

## Runtime 与 Flow

- v1.x 使用 conservative GC；v1.3.5 起默认执行经 Compiler/Verifier 验证的 Bytecode，但没有 JIT 或本地机器码编译，AST evaluator 永久保留为语义 oracle。
- Stream 单次消费；不能复制、比较、序列化或在多个下游重复使用。
- sort_by、group_by、collect、reduce 等屏障会物化有界输入，并受集合与内存上限约束。
- parallel 使用隔离 worker 进程并保持输入顺序，适合 I/O/批处理，不承诺线程级低延迟。
- Result、File、Directory、FileEvent、Process、CommandResult、HTTP 对象和其他系统对象不能直接编码为 JSON；应先 map/pick 成普通 Map/List/标量。

## 优化编译器与 Runtime 实验

- v1.7.0 已交付结构化 HIR、六个静态 pass、有界整数 MIR、参数反馈 guard/deopt 和局部 List 标量替换；默认仍为直接 Bytecode 编译器。跨平台正确性验收通过不等于真实性能默认准入。
- `HHY_COMPILER=ir` 显式选择 IR 编译；`HHY_FEEDBACK_SPECIALIZATION=1` 启用类型特化；`HHY_SCALAR_REPLACEMENT=1` 配合类型特化启用符合条件的 List 标量替换。未知类型、捕获、动态调用与可能逃逸的聚合值回退通用执行。
- MIR 只覆盖受支持的有界直线整数表达式；未提供通用 SSA/phi、循环展开、循环不变量外提、JIT 或机器码后端。
- List 标量替换保留原始托管存储预约、扫描类型及存活期，不代表物理堆分配消除或内存节省。
- 调用布局、异常表、帧池、调用展开与 Map inline cache 实验独立开关且默认关闭。尚未准入 superinstruction、持久名字/builtin 缓存、PIC/hidden shape、新 GC 或 async scheduler。
- 优化开关、回退方法和观测入口见 [1.7.0 使用指南](CURRENT_VERSION.md)；算法与预算见 [Compiler IR](architecture/COMPILER_IR.md)。

## I/O、网络与数据

- 文本、源码、路径、环境变量和命令文本必须是有效 UTF-8；任意二进制数据使用 BytesBuffer。
- `send` 仍将 HTTP response body 有界缓冲到内存；`send_to` 可直接流式写入同目录临时文件并原子发布，但两者都受默认 16 MiB body 上限约束。HTML 解析仍会把单页源码物化为 String。
- HTTP 使用 libcurl 和系统 CA，HHY 不提供自定义 TLS 实现。
- 通用 HTTP 为兼容现有本机自动化默认允许私网；面向不可信 URL 的抓取器必须显式设置 `allow_private_networks: false`，官方 my-crawler 已默认启用该保护。
- Regex 使用 PCRE2，并受 pattern、subject、match、depth、heap 和 capture 上限约束。
- CSV 支持流式 record，但 v1.0 不进行 schema 推断或自动数值类型转换。
- files 默认不跟随目录符号链接；显式开启后仍会检测并跳过目录循环。
- 官方 my-crawler 的 checkpoint 是单进程、批次边界的完整状态快照，不是分布式队列或并发写入协议；恢复会校验行为与安全配置指纹。
- JavaScript 渲染由隔离的可选 Playwright renderer 完成，需要 Node.js 与 Chromium。renderer 会检查主文档、重定向和子资源的域名及 DNS 地址，但 DNS 检查不能替代操作系统级网络沙箱；处理敌对页面时应额外使用容器或网络命名空间限制出口。

## 扩展与兼容

- 当前支持本地安装、签名 Registry、依赖解析、锁定与回滚；公开 Native ABI 未开放。
- Process Extension Protocol v1 支持握手、动态 callable 注册、普通调用、结构化错误和关闭；DB 1.0 已接入作用域句柄、取消和宿主游标 Stream；通用 Stream credit 与通用原生句柄 ABI 未开放。
- Database 1.0.0 支持 MySQL/PostgreSQL 池、远程 TLS、读写事务、保存点、预处理和增量游标。旧 URL 仅支持 loopback，远程使用配置 Map 与精确 allow 端点；Windows 包不含 DB。
- DB 字段上限 64 KiB、消息 1 MiB、有界 query 最多 10000 行；事务/游标有租期。提交结果不确定时不能自动重放写入。
- DB 本地与 CI 双数据库验收通过，RDS 实机和 24 小时长稳尚未验证。完整边界见 [DB README](../extensions/database/README.md)。
- Core 内部 Contract Registry 仍不是稳定 C ABI；第三方扩展必须使用进程协议。
- v1.x 可以新增非关键字 API，但不得改变合法 v1.0 程序的既有语义。

## 验证环境说明

- Apple Silicon 上的 Docker x86_64 翻译后端与 BDWGC + ASan 地址空间初始化不兼容；该组合在进入 HHY 测试前失败。因此翻译容器只用于 Linux x86_64 Release 验证，不能替代真实 Linux x86_64 runner 的 sanitizer/CI 发布证据。
- 原生 Linux x86_64 的 BDWGC 在 ASan 下执行极低内存 unwind 用例时会在 `GC_malloc_kind_global` 内崩溃。该平台因此运行 UBSan 完整测试、Release 完整测试以及不依赖该低内存路径的 ASan+UBSan coverage fuzz；Linux arm64 与 macOS arm64 继续运行 ASan+UBSan 完整测试。
