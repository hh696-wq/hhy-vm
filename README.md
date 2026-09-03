<div align="center">
  <img src="assets/brand/hhy-logo-option-3.png" width="260" alt="HHY Language logo">

  # HHY Language

  **Pipe Everything.**

  A small, deterministic, flow-first language for files, processes, HTTP, data pipelines, and safe static crawling.

  [5-minute Quick Start](https://hhylang.dev/zh/learn/quick-start) · [60–90s terminal demo](docs/TERMINAL_DEMO.md) · [Website](https://hhylang.dev) · [Specification](docs/HHY_V1.md)

  [![Version](https://img.shields.io/badge/version-1.4.3-0969da)](VERSION)
  [![CI](https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml/badge.svg)](https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml)
  [![License](https://img.shields.io/badge/license-Apache--2.0-0b7285)](LICENSE)
</div>

HHY 是一门用 C 从零实现的系统脚本语言。它用同一种 `source |> transform |>
action` 模型连接文件、进程、网络与结构化数据，并提供有界并发、资源限制、
脱敏 dry-run 和扩展权限清单。它不是自然语言或 AI 包装层，脚本具有确定的
grammar、类型规则、执行语义和退出码。

## 30 秒开始

macOS arm64、Linux x86_64 / arm64：

```sh
curl -fsSL https://hhylang.dev/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
hhy --version
```

macOS arm64 也可以使用仓库内维护的 Homebrew Tap：

```sh
brew tap hh696-wq/hhy https://github.com/hh696-wq/hhy-vm.git
brew install hhy
```

安装器会选择平台包、下载同名 `.sha256`、强制校验后安装到
`~/.local/share/hhy/<version>`，并在 `~/.local/bin` 创建入口。可审计源码见
[install.sh](install.sh)，手动下载见 [Releases](https://github.com/hh696-wq/hhy-vm/releases/latest)。

```hhy
processes
    |> where { process -> process.memory > 1gb }
    |> sort_by({ order: "desc" }) { process -> process.memory }
    |> take(10)
    |> print
```

它的核心表达始终是：

```text
source |> transform |> filter |> action
```

## 为什么是 HHY

- **Flow-first**：`|>` 不是附加语法，而是标准库和运行时共同遵循的执行模型。
- **系统能力是一等公民**：直接处理文件、目录、文本、JSON、CSV、进程、HTTP 和文件监听。
- **面向真实脚本**：具有变量、集合、函数、闭包、分支、循环、模块和结构化错误处理。
- **原生单位**：直接书写 `10mib`、`500ms`、`2h` 和 `80%`，避免隐含换算。
- **可预测执行**：惰性 Stream、有界并发、背压、取消、资源限制和稳定退出码。

## Web Runtime（v1.4）

HHY 可作为常驻 Web Runtime 使用：应用每个 Worker 只加载一次，Router、JSON API、
Middleware、静态文件、上传、CORS、gzip、SSE、流式响应、Range、多 Worker、健康检查与
Prometheus 指标均由 Runtime 提供。

```sh
./build/hhy serve examples/10-web-api.hhy -- 8080
./build/hhy serve --dev examples/10-web-api.hhy -- 8080
```

完整 API、反向代理部署边界和 C 嵌入示例见 [Web Runtime 文档](WEB_RUNTIME.md)。

## 快速开始

当前版本是 **V1.4.3**（`1.4.3`），正式支持 macOS arm64、Linux arm64 和
Linux x86_64；Windows x86_64 通过 MSYS2 执行构建与核心 Runtime 验证。

### 一键安装（推荐）

```sh
curl -fsSL https://hhylang.dev/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
hhy --version
```

完整的 5 分钟路径、手动校验和卸载说明见
[Quick Start](https://hhylang.dev/zh/learn/quick-start)。

### 从源码构建

需要 C11 编译器、`make`、libcurl、PCRE2、BDWGC、Jansson 和 OpenSSL。macOS 可以先安装依赖：

```sh
brew install curl pcre2 bdw-gc jansson openssl@3 zlib
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

## 实战项目

完整、经过端到端自测的 HHY 应用统一收录在
[practical-projects](practical-projects/README.md)：

- [FlowGuard](practical-projects/flowguard/README.zh-CN.md)：面向 CI/CD 的项目质量门禁；
- [DataFlow ETL](practical-projects/dataflow-etl/README.zh-CN.md)：CSV/JSON 数据补全与报表管道；
- [Asset Governance](practical-projects/asset-governance/README.zh-CN.md)：安全的资产审计与整改工作流。
- [香港电影公司](practical-projects/hong-kong-film-companies/README.zh-CN.md)：并发抓取维基百科并汇总为 CSV。
- [多 API 数据采集器](practical-projects/multi-api-data-collector/README.zh-CN.md)：并发采集 OpenAlex、Crossref 与 GitHub，统一并增量汇总为 CSV。

## 下载并运行

从 [GitHub Releases](https://github.com/hh696-wq/hhy-vm/releases/latest)
下载与你的系统和 CPU 架构匹配的压缩包。发行包包含所需的非系统运行库，解压后
保持 `bin/` 与 `lib/` 的相对位置不变即可直接运行：

```sh
tar -xzf hhy-1.4.3-PLATFORM-ARCH.tar.gz
cd hhy-1.4.3-PLATFORM-ARCH
./bin/hhy --version
./bin/hhy run examples/07-language-basics.hhy
```

发布页同时提供逐文件 `.sha256` 和汇总 `SHA256SUMS`，运行前应先校验下载内容。
从源码构建和系统级安装方式见 [INSTALL.md](INSTALL.md)。

## 命令行工具

| 命令 | 用途 |
|---|---|
| `hhy run <script.hhy> [args...]` | 运行脚本并传递参数 |
| `hhy profile <script.hhy> [args...]` | 分析脚本 CPU 热点、调用次数和托管 Heap 分配 |
| `hhy repl` | 启动交互式环境 |
| `hhy check <file.hhy>...` | 检查语法和核心语义 |
| `hhy check --format json <file.hhy>...` | 输出版本化、机器可读的结构化诊断 |
| `hhy contracts --format json` | 输出工具可消费的 Callable Contract Registry |
| `hhy fmt <file.hhy>...` | 格式化源码 |
| `hhy ast <file.hhy>` | 输出抽象语法树 |
| `hhy bytecode <file.hhy>` | 编译、验证并反汇编实验性 Bytecode IR；不执行 Bytecode |
| `hhy bytecode --metrics <file.hhy>` | 输出 compile/verify/prepare 的机器可读缓存准入测量 |
| `hhy tokens <file.hhy>` | 输出词法 Token |
| `hhy run --dry-run <file.hhy>` | 生成脱敏执行计划，不执行外部副作用 |
| `hhy install <local-path>` | 在 staging 校验权限与 SHA-256 后原子安装本地进程扩展 |
| `hhy install --registry DIR --trust-root FILE <namespace/name>` | 验签、解析依赖并事务式安装官方扩展 |
| `hhy lock --registry DIR --trust-root FILE <namespace/name>` | 写入精确签名索引快照与宿主 target 的 `hhy.lock` |
| `hhy fetch --locked ...` | 把锁定图写入内容寻址离线缓存并逐文件复核 SHA-256 |
| `hhy install --locked --offline ... <namespace/name>` | 只从锁定缓存验签并重建相同依赖图 |
| `hhy install --upgrade ...` / `hhy rollback <package>` | 原子升级并恢复上一已验证版本 |
| `hhy doctor extensions` | 检查 active installation；可同时核对 lock 与 cache |
| `hhy list` | 列出已安装扩展 |
| `hhy remove <package>` | 移除扩展包 |

运行 `hhy --help` 查看完整参数和资源限制选项。

VS Code 0.2.0 通过内置 HHY Language Server 提供实时诊断、标准格式化、
本地定义跳转、Contract-aware Hover 与补全。若 `hhy` 不在 `PATH`，可设置
`hhy.executablePath`。语言服务器直接消费上述 JSON CLI contract，避免复制
Parser、Checker 或 Callable Registry 语义。

性能分析默认把报告写入 stderr，保持脚本 stdout 不变：

```sh
hhy profile script.hhy -- input
hhy profile --cpu script.hhy
hhy profile --heap --format json --output profile.json script.hhy
```

CPU 数据以 1ms 进程 CPU 时间采样，阻塞等待不会被算作 CPU 热点；很短的脚本可能
没有足够样本。Heap 数据统计 HHY 托管内存的累计申请、观察峰值和 GC 后占用，
不包含扩展进程以及 libcurl 等原生库自行管理的内存。

## 进程扩展与签名 Registry

本地开发安装仍会展示 capability，安装后和每次加载前都会
校验 manifest 与可执行文件的 SHA-256：

```sh
make -C extensions/database
./build/hhy install ./extensions/database
./build/hhy list
```

`install` 与 `list` 会展示作者、协议和完整 capability，方便安装前确认来源与
最小权限。官方包使用 `author = "HHY Official"`；本地 manifest 的作者字段是
署名信息，不能替代 Registry 的发布者签名。

V1.2.1 在 V1.2.0 静态签名 Registry 上增加 lockfile、内容寻址离线缓存、可复现安装、
原子升级历史和显式回滚。客户端在任何写入之前验证 Ed25519
索引签名、包描述签名、完整文件 SHA-256 清单和传递依赖图；`--dry-run` 只输出确定性
计划。协议与安全边界见 [`docs/EXTENSION_REGISTRY_V1.md`](docs/EXTENSION_REGISTRY_V1.md)。
线上域名不是本地实现和 CI 验收的前置条件。

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

HTML 扩展使用 Lexbor 对不可信 HTML 进行 HTML5 容错解析和 CSS Selector 查询，
自身不访问网络；抓取仍由 Runtime 的 HTTP、timeout、retry、TLS 与 dry-run 负责：

```sh
make -C extensions/html
./build/hhy install ./extensions/html
```

```hhy-snippet
import html

html.extract_report(body, "article.product", {
    title: { selector: "h2", value: "text" },
    url: { selector: "a", value: "attr", name: "href" }
})
```

API、结果上限和构建依赖见 [`extensions/html`](extensions/html/README.md)。

## V1.3.10 Bytecode 状态

V1.0 语言 contract 保持兼容；V1.3.0 正式提供 `hhy run --engine bytecode`，V1.3.1
增加官方真实负载的双引擎兼容矩阵，V1.3.5 默认启用 Compiler/Verifier 验证的 Bytecode 路径，并固化 Runtime 内部边界、
版本化治理检查和机器可读工作负载证据。V1.3.7 将 Stream Int fast path 的 magic kind
替换为具名操作和集中 metadata，提供稳定 fallback reason、JSON 选择证据、内部关闭开关。
V1.3.8 将表达式特化识别从 Runtime 前移到 Compiler，生成版本化 Stream Kernel IR；
独立 Verifier 检查 opcode、类型流、stack shape、常量与 RETURN，反汇编展示 kernel 计划，
Runtime 只执行已验证 kernel，未知或动态 closure 无损回退通用 Bytecode，并以三路径差分、
变形、损坏计划和 fuzz 持续验证。V1.3.9 让普通执行与 Profiler 共用同一优化选择，
Profile JSON v2 记录选择原因、版本化 kernel 与 opcode，CPU/Heap 热点归因融合执行，
并以取消一致性和机器可读开销门禁持续验证。V1.3.10 对五个固定真实负载执行配对冷进程
测量；compile+verify 中位数仅占冷运行墙钟的极小部分，未达到 `1 ms` 且 `20%` 的缓存
准入双门槛。因此本版本不引入进程内或磁盘 Bytecode 缓存，也不接受外部预编译 Bytecode；
机器可读决策门禁固化未来缓存必须绑定源码与递归依赖摘要、语言/Bytecode/Kernel 版本、
编译 feature、target 与安全策略，并在 checksum、有界解析、完整 Verifier 和执行计划验证后
才能执行，任何陈旧、损坏或未知数据都必须安全重编译；
有界 instruction/frame/operand 执行计划、Profiler engine/source 标记、HHY Stack trace、
故障注入以及完整 AST/Bytecode 双引擎测试。共享 Runtime 继续唯一负责 closure、异常、
GC、取消、Stream、effect 和资源限制。默认执行引擎为 Bytecode，并永久保留 `--engine ast` 回退；
Bytecode 不提供持久磁盘格式或公开 ABI。V1.2.2 使用官方 HTML 0.2.0 扩展验证真实网页批量抽取、
可观察截断、结构化扩展错误以及三平台依赖发行，现有 Protocol 1 同步批量路径足以满足
当前有界负载，因此不提前加入 Stream credit 或 Opaque Handle。V1.2.1 完成官方扩展签名 Registry、确定性依赖解析、
事务式安装、锁定、离线重建与安全回滚闭环。V1.1.8 增加的首个 Runtime
模块边界、内部所有权 API、静态治理检查和阻断式性能回归门禁，公开行为与默认资源限制不变。
V1.1.7 提供版本化 JSON diagnostics、Contract Registry JSON 和最小 LSP/VS Code
编辑闭环。V1.1.6 已补齐测试能力探测、
分层 CI、机器可读性能基线与发布一致性门禁。V1.1.5 已完成持久 Frontier、
批次 checkpoint 与断点恢复、原子流式 HTTP 落盘和独立 Playwright JavaScript 渲染器，
并增加 Windows x86_64 MSYS2 构建验证。V1.1.3 完成 GC 引用缓冲、Stream 去重状态、Map/JSON
哈希索引、进程快照诊断和 Profiler 数据质量标记的技术评审修复，并继续包含 V1.1.2
的官方 HTML 扩展与静态文档采集框架。
内建 `hhy profile` 提供 CPU 与托管 Heap 分析；内存、递归、集合及运行时上限通过
压力测试确保越界返回稳定错误而不是使 Runtime 崩溃。每次主分支提交都会在 macOS arm64、
Linux arm64、Linux x86_64 和 Windows x86_64 MSYS2 上执行相应的严格编译与验证；
三个正式发行平台继续执行 sanitizer、完整测试、fuzz、
文档验证、发行包内容检查和 SHA-256 校验。

- 当前版本来源：[VERSION](VERSION)
- 语言手册与标准库：[hhylang.dev/zh/learn](https://hhylang.dev/zh/learn)
- Bytecode 与 CLI：[CLI Reference](https://hhylang.dev/zh/learn/cli-reference)
- 当前健康状态：[Language Health Report](https://hhylang.dev/zh/learn/language-health-report)
- 四平台持续验证：[GitHub Actions](https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml)

## 文档

- [HHY 在线语言手册](https://hhylang.dev/zh/learn)
- [安装说明](INSTALL.md)
- [典型脚本](examples/README.md)
- [扩展系统](https://hhylang.dev/zh/learn/extensions)

## 许可证

HHY Language 使用 [Apache License 2.0](LICENSE)。允许使用、修改、分发、
商用和二次开发；重新分发时必须遵守许可证中的署名、变更说明和 `NOTICE`
保留要求。第三方依赖继续适用各自的许可证。

---

<div align="center">
  <strong>Built solo. Designed to flow.</strong>
</div>
