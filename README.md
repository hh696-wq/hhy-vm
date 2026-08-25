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

当前状态：v1.0 实现与规范收敛阶段（开发版本仍为 `0.5.0-dev`，尚未冒充稳定版）。核心语言、惰性 Flow、系统 I/O、网络、watch、取消和有界保序 parallel 已经可以真实执行。

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

当前 Runtime 已支持：

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

尚未完成的 v1.0 发布闭环主要是原生 Linux x86_64 sanitizer/CI、macOS CI 与最终 contract 冻结。Linux arm64 的 Debug/Release、完整测试、覆盖引导 libFuzzer 和发行包已经实测通过；Linux x86_64 的 Release、完整测试和发行包已在模拟环境通过，但 QEMU 不能替代原生 sanitizer 证据。String 与 Map 已使用显式长度并覆盖嵌入 `U+0000` 的回归测试；结构化错误、资源 unwind、五类核心验收程序和 checksummed archive 均已进入回归套件。当前 parallel 使用隔离 worker 进程。项目不会在剩余证据完成前标记 v1.0。

## 文档

- [HHY Language v1.0 统一规范](docs/HHY_V1.md)
- [HHY v1.0 已知限制](docs/KNOWN_LIMITATIONS.md)
- [第三方依赖与许可证边界](docs/DEPENDENCIES.md)
- [HHY 扩展系统版本路线图](docs/EXTENSION_ROADMAP.md)
- [HHY v1.0 典型脚本](examples/README.md)
