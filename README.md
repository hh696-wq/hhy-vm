<div align="center">
  <img src="assets/brand/hhy-logo-option-3.png" width="260" alt="HHY Language logo">

  # HHY Language

  **Pipe Everything.**

  A flow-first scripting language for system automation.

  [Website](https://hhylang.dev) · [Language Specification](docs/HHY_V1.md) · [Examples](examples/README.md) · [Known Limitations](docs/KNOWN_LIMITATIONS.md)

  [![Version](https://img.shields.io/badge/version-1.1.0-0969da)](VERSION)
  [![CI](https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml/badge.svg)](https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml)
  [![License](https://img.shields.io/badge/license-Apache--2.0-0b7285)](LICENSE)
</div>

HHY 是一门用 C 从零实现的系统脚本语言。它通过统一的 Flow 模型连接文件、
进程、网络和结构化数据，让系统自动化像描述数据流一样直接。

```hhy
processes
    |> where { process -> process.memory > 1gb }
    |> sort_by({ order: "desc" }) { process -> process.memory }
    |> take(10)
    |> print
```

HHY 不是自然语言理解系统，也不依赖 AI。源码具有确定的 grammar、类型规则
和执行语义。它的核心表达始终是：

```text
source |> transform |> filter |> action
```

## 为什么是 HHY

- **Flow-first**：`|>` 不是附加语法，而是标准库和运行时共同遵循的执行模型。
- **系统能力是一等公民**：直接处理文件、目录、文本、JSON、CSV、进程、HTTP 和文件监听。
- **面向真实脚本**：具有变量、集合、函数、闭包、分支、循环、模块和结构化错误处理。
- **原生单位**：直接书写 `10mib`、`500ms`、`2h` 和 `80%`，避免隐含换算。
- **可预测执行**：惰性 Stream、有界并发、背压、取消、资源限制和稳定退出码。

## 快速开始

当前稳定版本是 **V1.1.0**（`1.1.0`），正式支持 macOS arm64、Linux arm64 和
Linux x86_64。

### 从源码构建

需要 C11 编译器、`make`、libcurl、PCRE2、BDWGC、Jansson 和 OpenSSL。macOS 可以先安装依赖：

```sh
brew install curl pcre2 bdw-gc jansson openssl@3
```

然后构建并验证：

```sh
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm
make
./build/hhy --version
make test
```

### 安装

```sh
make install PREFIX="$(brew --prefix)"
hhy --version
```

其他安装位置和 Linux 说明见 [INSTALL.md](INSTALL.md)。

### 运行第一个脚本

创建 `hello.hhy`：

```hhy
let language = "HHY"

["Flow", "Pipe", "System"]
    |> map { word -> "{language}: {word}" }
    |> print
```

运行：

```sh
hhy run hello.hhy
```

也可以直接执行仓库中的示例：

```sh
hhy run examples/07-language-basics.hhy
```

## 语言一览

### 文件与文本

```hhy
path("./logs")
    |> files("**/*.log")
    |> flat_map { file -> read_lines(file.path) }
    |> where { line -> contains(line, "ERROR") }
    |> take(20)
    |> print
```

### HTTP 与 JSON

```hhy
http.get("https://example.com/users")
    |> timeout(5s)
    |> retry(3)
    |> send
    |> response_body
    |> parse_json
    |> print
```

### 有界并发

```hhy
["https://example.com", "https://example.org"]
    |> parallel(2) { url ->
    http.get(url)
        |> timeout(5s)
        |> send
}
    |> print
```

更多可执行场景见 [examples](examples/README.md)。README 与规范中的完整
HHY 代码块都会由 CI 送入 Parser 和 Checker，避免文档示例与语言实现脱节。

## 下载并运行

从 [GitHub Releases](https://github.com/hh696-wq/hhy-vm/releases/latest)
下载与你的系统和 CPU 架构匹配的压缩包。发行包包含所需的非系统运行库，解压后
保持 `bin/` 与 `lib/` 的相对位置不变即可直接运行：

```sh
tar -xzf hhy-1.1.0-PLATFORM-ARCH.tar.gz
cd hhy-1.1.0-PLATFORM-ARCH
./bin/hhy --version
./bin/hhy run examples/07-language-basics.hhy
```

发布页同时提供逐文件 `.sha256` 和汇总 `SHA256SUMS`，运行前应先校验下载内容。
从源码构建和系统级安装方式见 [INSTALL.md](INSTALL.md)。

## 命令行工具

| 命令 | 用途 |
|---|---|
| `hhy run <script.hhy> [args...]` | 运行脚本并传递参数 |
| `hhy repl` | 启动交互式环境 |
| `hhy check <file.hhy>...` | 检查语法和核心语义 |
| `hhy fmt <file.hhy>...` | 格式化源码 |
| `hhy ast <file.hhy>` | 输出抽象语法树 |
| `hhy tokens <file.hhy>` | 输出词法 Token |
| `hhy run --dry-run <file.hhy>` | 生成脱敏执行计划，不执行外部副作用 |
| `hhy install <local-path>` | 校验权限与 SHA-256 后安装本地进程扩展 |
| `hhy list` | 列出已安装扩展 |
| `hhy remove <package>` | 移除扩展包 |

运行 `hhy --help` 查看完整参数和资源限制选项。

## 本地进程扩展

V1.1 只从本地路径安装扩展。安装前会展示 capability，安装后和每次加载前都会
校验 manifest 与可执行文件的 SHA-256：

```sh
make -C extensions/database
./build/hhy install ./extensions/database
./build/hhy list
```

`install` 与 `list` 会展示作者、协议和完整 capability，方便安装前确认来源与
最小权限。官方包使用 `author = "HHY Official"`；本地 manifest 的作者字段是
署名信息，不能替代未来远程仓库的发布者签名。

数据库连接信息由脚本显式读取并只作为该次调用参数传给隔离扩展进程：

```hhy-snippet
import database

let url = require_env("DATABASE_URL")
database.query(url, "SELECT id, name FROM users WHERE active = $1", [true], 100)
    |> get("rows")
    |> print

database.transaction(url, [
    { sql: "INSERT INTO audit_log (id, message) VALUES ($1, $2)", params: [1, "created"] },
    { sql: "UPDATE audit_log SET message = $1 WHERE id = $2", params: ["committed", 1] }
])
```

PostgreSQL 使用 `$1` 参数，MySQL 使用 `?`。事务第一版在同一连接上原子执行
1–100 条 `INSERT`、`UPDATE` 或 `DELETE`，任一失败会整体回滚。完整示例和构建依赖见
[`extensions/database`](extensions/database/README.md)。

## V1.1.0 状态

V1.0 语言 contract 保持兼容；V1.1 加入本地 Process Extension Protocol。每次主分支提交都会在 macOS arm64、
Linux arm64 和 Linux x86_64 上执行严格编译、sanitizer、完整测试、fuzz、
文档验证、发行包内容检查和 SHA-256 校验。

- 当前版本来源：[VERSION](VERSION)
- 语言的唯一规范来源：[docs/HHY_V1.md](docs/HHY_V1.md)
- 已知限制：[docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md)
- 三平台持续验证：[GitHub Actions](https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml)

## 文档

- [HHY Language V1.0 统一规范](docs/HHY_V1.md)
- [安装说明](INSTALL.md)
- [典型脚本](examples/README.md)
- [扩展系统路线图](docs/EXTENSION_ROADMAP.md)
- [第三方依赖与许可证](docs/DEPENDENCIES.md)
- [第三方许可证声明](docs/THIRD_PARTY_NOTICES.md)

## 许可证

HHY Language 使用 [Apache License 2.0](LICENSE)。允许使用、修改、分发、
商用和二次开发；重新分发时必须遵守许可证中的署名、变更说明和 `NOTICE`
保留要求。第三方依赖继续适用各自的许可证。

---

<div align="center">
  <strong>Built solo. Designed to flow.</strong>
</div>
