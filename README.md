# HHY

> **Pipe Everything.**

A lightweight, flow-first scripting language for system automation.

**Built solo. Designed to flow.**

Official website: [hhylang.dev](https://hhylang.dev)

```hhy
processes
    |> where { process -> process.memory > 1gb }
    |> sort_by({ order: "desc" }) { process -> process.memory }
    |> take(10)
    |> print
```

HHY 通过统一的管道模型连接文件、进程、网络与结构化数据，让系统自动化像描述数据流一样简单。

HHY 不是自然语言理解系统，也不依赖 AI。每条语句都有确定的 grammar、类型规则和执行语义，使用 C 从零实现。

## 当前版本与 v1.0 进度

| 项目 | 当前状态 |
|---|---|
| 当前稳定版本 | `1.0.0`（与仓库根目录 `VERSION` 一致） |
| 版本目标 | `1.0.0` Stable 已冻结 |
| 实现进度 | v1.0 规范内的语言、Flow、系统标准库与工具链已经落地 |
| 验证进度 | `1.0.0` 在 macOS arm64、Linux arm64、Linux x86_64 的统一公开门禁已经通过 |
| 发布状态 | v1.0 contract、版本与支持矩阵已经冻结并验证 |

v1.0 的目标不是堆叠功能数量，而是交付一门可以安装、可以编写真实系统脚本、
错误与资源行为可预测的 Flow-first 语言：

- 完整动态脚本语言：变量、作用域、集合、函数、闭包、分支、循环、模块与错误处理。
- 统一 Flow：`source |> transform |> filter |> action`，惰性、拉取式、单次消费。
- 系统一等公民：文件、目录、文本、JSON、CSV、Regex、进程、HTTP、watch 与原生单位。
- 有界并发：保序 `parallel(n)`、背压、失败取消、隔离 worker 与 Sendable 校验。
- 可发布工具链：`run`、`repl`、`fmt`、`check`、dry-run Execution Plan、资源限制和稳定退出码。
- 正式支持 macOS arm64、Linux arm64、Linux x86_64，并提供带 SHA-256 的安装包。

## 构建与检查

构建需要 C11 编译器、`make`、libcurl、PCRE2 和 BDWGC。macOS 可执行
`brew install curl pcre2 bdw-gc`，Apple Clang 可直接编译：

```sh
make
./build/hhy check examples/*.hhy
./build/hhy ast examples/01-log-errors.hhy
./build/hhy run examples/07-language-basics.hhy
./build/hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
make test
make test-debug
```

安装为全局 `hhy` 命令（Apple Silicon Homebrew 环境）：

```sh
make install PREFIX="$(brew --prefix)"
hhy run examples/00-hello.hhy
```

当前可用命令：

- `hhy check <file.hhy>...`：语法与核心语义检查
- `hhy ast <file.hhy>`：输出 AST
- `hhy tokens <file.hhy>`：输出 Token
- `hhy fmt <file.hhy>...` / `hhy fmt --check <file.hhy>...`：格式化或验证格式
- `hhy repl`：启动持久作用域、多行输入 REPL
- `hhy run <file.hhy> [args...]`：执行脚本并传入参数
- `hhy --version`：输出版本
- `hhy run --dry-run <file.hhy>`：通过 Contract Registry 输出脱敏 Execution Plan，并拦截文件写入、进程启动和网络发送

## v1.0 已完成的实现

- 变量、块级作用域、函数、闭包、条件、循环和错误传播
- List、Map、Result、File/Directory/FileEvent、Process/CommandResult、Path、Bytes、Duration、Percent 和基础运算
- Checker/Runtime 共用的 Callable Contract Registry，以及统一 EffectDispatcher 和 Error stage
- 惰性、拉取式、单次消费 Stream，以及 `map/where/take/collect/count/sum/for_each`
- `read_text/read_lines/write_text/append_text/save_text/save_lines/files`
- UTF-8/Unicode 文本、严格 JSON 解析与编码、流式 CSV（header/delimiter/quote）
- argv 直传的 `run` 和 `stdout_lines`
- `http.get/post/put/delete`、`timeout/retry/send/response_body`
- Range、高级 Flow barrier、Regex 与流式 CSV
- `env`、`system`、`processes`
- Pure HHY Module：相对 import、具名导入、别名、export、缓存和循环检测
- 不可变集合更新、Path 操作和 copy/move/remove
- `hhy script.hhy` 与 `hhy run script.hhy` 两种执行入口
- 统一 Ctrl+C CancellationToken，子进程、HTTP 和 Stream 共同响应
- `every(Duration)` 无限 Tick Stream
- `watch(Path)` 递归文件事件 Stream
- `parallel(n)` 有界并发、输入顺序输出、失败取消和跨 worker 值校验
- Checker 对 Parallel mutable/Stream 捕获执行静态 Sendable 拒绝
- `sleep(Duration)` 及统一取消响应
- `run(args, options)` 的 cwd、env、stdin、timeout 与 max_output
- HTTP 16 MiB 响应限制、Flow/JSON 集合物化上限和全局 parallel 上限
- HTTP query/header/body/proxy/redirect、二进制响应和写入
- Path 词法规范化与可选的循环安全 symlink 遍历
- 即时错误与惰性错误统一 `on_error` 恢复
- 规范化 CLI 退出码 `0–5`

## v1.0 发布验证

- `HHY_V1.md` 第 36 节发布条件已经逐项审计，核心 contract 已冻结。
- 三个正式平台已在同一公开 CI 上完成 sanitizer、Release、fuzz、文档与制品验证。
- `VERSION`、文档、`hhy --version` 和安装包统一为 `1.0.0`。
- `1.0.0` 冻结提交已通过同一三平台门禁并生成三个带 SHA-256 的制品。

`1.0.0` 冻结提交的三平台全绿证据见
[GitHub Actions #11](https://github.com/hh696-wq/hhy-vm/actions/runs/32817348334)。

## 文档

- [HHY Language v1.0 统一规范](docs/HHY_V1.md)
- [HHY v1.0 已知限制](docs/KNOWN_LIMITATIONS.md)
- [第三方依赖与许可证边界](docs/DEPENDENCIES.md)
- [HHY 扩展系统版本路线图](docs/EXTENSION_ROADMAP.md)
- [HHY v1.0 典型脚本](examples/README.md)
