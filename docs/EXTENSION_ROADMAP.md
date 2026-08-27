# HHY 扩展系统版本路线图

> 状态：规划稿 v0.1  
> 当前 Core 稳定版本：`1.1.2`（以仓库根目录 `VERSION` 为准）
> 依赖规范：[HHY Language v1.0 统一规范](HHY_V1.md)  
> 原则：HHY Core 负责 Flow，扩展负责提供可以流动的新数据。

本文档只描述扩展体系的版本、交付物和验收条件。语言语法、值模型、Stream、Error 和 Pipe 语义以 `HHY_V1.md` 为唯一来源。

## 1. 总体路线

```text
v1.0  Core contracts + namespace reservation
v1.1  Process Extension Protocol + local package install
v1.2  Official office extension validates the protocol
later Public Native ABI after Runtime stabilization
```

中文定义：

```text
v1.0  核心 contract 与命名空间预留
v1.1  Process Extension Protocol + 本地包安装
v1.2  官方 office 扩展验证协议
以后   Runtime 稳定后再考虑公开 Native ABI
```

这条路线有两个目标：

1. v1.0 不被插件系统拖慢，优先完成语言与 Flow Runtime。
2. v1.0 的内部边界足够清晰，v1.1 不需要破坏 Pipe、Value、Stream 和 Error 才能加入扩展。

## 2. v1.0：核心 Contract 与命名空间预留

v1.0 不加载第三方扩展、不提供包管理器、不公开进程协议，也不承诺 Native ABI。v1.0 的任务是让核心内部采用未来可扩展的统一 contract。

### 2.1 当前已经定义的核心 Contract

#### Pipe Contract

```text
x |> f           => f(x)
x |> f(a, b)     => f(x, a, b)
x |> obj.f(a)    => obj.f(x, a)
```

扩展函数未来不需要特殊 Pipe 语法。只要注册为 callable，就遵守相同的参数注入规则。

#### Value Contract

HHY Core 已定义这些逻辑值类别：

```text
Null Bool Int Float String Regex BytesBuffer
List Map Range Function Error Result Stream
Bytes Duration Percent DateTime Path
File Directory FileEvent Process CommandResult
HttpRequest HttpResponse
```

扩展未来可以提供两种值：

- Serializable value：能转换成 HHY 标准值或协议值。
- Opaque handle：数据只存在于扩展内部，HHY 只持有受控句柄。

v1.0 只冻结逻辑行为，不冻结 `HhyValue` 的公开二进制布局。

#### Function Contract

所有核心函数和未来扩展函数都需要逻辑签名：

```text
qualified_name
input_contract
output_contract
lazy
effect
cancel
threading
```

即使 HHY 使用动态类型，Checker 与 Runtime 仍可以据此完成：

- 参数数量检查。
- 已知值类别检查。
- Pipe 执行计划展示。
- 副作用和 dry-run 判断。
- 不支持取消时的诊断。

v1.0 当前实现使用单一 Callable Contract Registry 登记 94 个核心 callable。
每项都包含具体输入 contract、输出 contract、effect、lazy、cancel、sendable、
action 与 `main | worker | isolated_process` threading 元数据；Runtime 启动校验与
`tests/check_contracts.py` 会拒绝缺项、占位类型、非法线程模型、重复登记以及
Registry/Runtime 实现不一致。v1.1 动态注册必须复用这一结构，不能绕过它。

#### Operator Contract

Flow operator 使用：

```text
open -> next* -> close
```

要求：

- Stream 是惰性、拉取式、单次消费。
- `next` 受背压控制。
- `close` 必须幂等。
- 提前终止、错误和取消都会调用 `close`。
- operator 声明是逐项、有状态、屏障还是终端算子。
- operator 声明是否产生副作用。

未来扩展返回的数据流必须包装成同一个 contract，不能自建另一套 Stream 模型。

#### Error Contract

```text
kind
code
message
source
stage
cause
stack
context
```

扩展错误未来必须转换成 HHY Error。扩展名称和操作名称写入 stage/cause，不能把内部崩溃伪装成普通 null。

#### Cancellation Contract

每次脚本执行只有一个根 CancellationToken，向下传播到：

```text
Stream
filesystem
HTTP
process
watch
parallel
future extensions
```

Ctrl+C、timeout、未处理错误和显式退出走同一取消与资源清理路径。

#### Effect Contract

核心 action 按副作用分类：

```text
none
filesystem
process
network
custom
```

所有文件写入、进程启动、HTTP send 和未来扩展 action 必须经过统一 effect 调度点。这样后续才能实现：

- dry-run。
- 权限确认。
- 审计日志。
- 资源限制。
- 扩展 capability。

#### Resource Contract

核心资源遵守：

- 显式 open/close 生命周期。
- close 幂等。
- Error、return、exit 和 cancel 都会释放资源。
- 资源句柄不能 JSON 序列化。
- Stream、打开文件和未来 Opaque handle 不能随意进入 Parallel worker。
- 系统资源释放不依赖 GC finalizer。

#### Module Contract

v1.0 模块系统已经支持带点限定名：

```hhy
import http
import office.excel
```

EBNF：

```ebnf
module_name = IDENT { "." IDENT } ;
```

未安装的合法限定模块产生 `ModuleNotFoundError`，不是语法错误。这保证 v1.1 添加包以后无需修改 Parser。

#### Checker Contract

Checker 至少能读取内部注册表中的：

- qualified name。
- 参数数量。
- input/output contract。
- effect。
- 是否惰性。
- 是否支持取消。
- 能否发送到 Parallel worker。

v1.0 的标准库也应通过内部注册表描述，避免 v1.1 为扩展另造一套元数据系统。

### 2.2 v1.0 命名空间预留

#### 语言保留名称

关键字、特殊变量与核心语法名称由语言保留，例如：

```text
let mut fn if else for while
return break continue try catch throw
import export true false null
it args env system
```

扩展不能注册这些名称。

#### 标准库名称

v1.0 已发布的裸模块名和顶层内建名归 HHY 标准库所有，例如：

```text
http
files processes watch
map flat_map where reduce parallel
```

扩展不能覆盖标准库名称。新增标准库名称需要进行兼容性审查，避免抢占已经允许的第三方命名空间。

#### 平台保留命名空间

```text
hhy.*
std.*
```

- `hhy.*` 保留给 Runtime、工具链和官方平台元数据。
- `std.*` 保留给未来标准库的完全限定形式。
- 用户包和第三方扩展不能使用这两个顶级名称。

#### 官方扩展命名空间

官方扩展使用经过登记的独立顶级包名，例如：

```text
office.excel
office.word
office.powerpoint
```

`office` 不属于 v1.0 标准库，也不会随 Core 安装。它只是为未来官方扩展预留的包名。

#### 第三方命名空间

v1.1 本地包使用 manifest 中唯一的 package name 作为顶级命名空间：

```text
acme.database
example.image
```

同一 Runtime 中两个包不能注册同一个顶级名称。v1.1 需要提供别名机制处理脚本内命名冲突，而不是允许后安装者覆盖前者。

### 2.3 v1.0 内部实现要求

- 标准库函数通过统一 Registry 注册，不在 Parser 中硬编码函数名称。
- Parser 只解析 qualified name，不判断模块是否安装。
- Checker 负责模块和 callable 解析。
- Execution Plan 记录 operator name、effect、lazy 和 cancel 属性。
- Runtime 所有副作用经过 EffectDispatcher。
- CancellationToken 能到达每个阻塞操作。
- Error 能记录当前 operator stage。
- Stream operator 不暴露具体标准库实现细节。
- HhyValue 内存布局保持内部私有。

### 2.4 v1.0 不提供

- `hhy install`。
- `hhy add`。
- `hhy.toml` 的正式解析和依赖解析。
- 远程包下载。
- 包锁文件。
- 第三方扩展进程启动。
- Extension Protocol 编解码。
- 公开 Native ABI header。
- Office 扩展实现。

### 2.5 v1.0 验收条件

- `import office.excel` 可以通过 Parser，但 Checker 返回稳定的 `ModuleNotFoundError`。
- 标准库函数全部能由同一个内部 Registry 查询。
- Execution Plan 能显示 Flow 阶段和 effect。
- dry-run 通过 EffectDispatcher 拦截所有核心 action。
- 自定义测试 operator 可以通过内部 contract 接入 Flow，而不修改 Parser。
- Ctrl+C 可以取消自定义测试 operator。
- HhyValue、Stream 和 Error 头文件未作为第三方稳定 ABI 发布。

当前 Core 实现状态：

- `src/contracts.c` 是 Checker 与 Runtime 共享的唯一 callable contract Registry，记录 qualified name、arity、effect、lazy、cancel、sendable 与 action。
- Registry 启动时验证名称唯一性和 arity 区间；Checker 同时检查裸 callable 与 `http.get`、`datetime.parse` 等 qualified callable。
- Runtime 的 EffectDispatcher 根据同一 contract 决定 dry-run 是否允许文件、进程和网络 action，并输出脱敏 Execution Plan。
- Runtime Error 的 `stage` 使用当前 callable contract 名称，而不是笼统 AST 节点名。
- Registry 和 EffectDispatcher 是 Core 内部实现，不随 v1.0 发行包发布为稳定 Native ABI。

动态注册第三方实现仍属于 v1.1 Process Extension；v1.0 的“自定义测试 operator”验收只验证 Core 内部 operator 可通过 contract 表加入、Parser 无需新增语法分支，不表示 v1.0 对外开放注册 API。

## 3. v1.1：Process Extension Protocol 与本地包安装

v1.1 第一次允许用户安装和运行第三方扩展，但首选独立进程隔离，不开放 Native ABI。

实现状态（`1.1.1`）：本地 `install/list/remove`、manifest/capability 展示、安装与
加载时 SHA-256 校验、进程握手、动态 callable 注册、同步 call/result/error 和
shutdown 已完成。协议线格式见 [`EXTENSION_PROTOCOL_V1.md`](EXTENSION_PROTOCOL_V1.md)。
Stream credit、Opaque handle 和跨调用 cancel 保留在后续 v1.x 增量，不属于当前
同步有界调用子集；`database` reference extension 用于验证 PostgreSQL/MySQL 集成。

### 3.1 交付内容

- `hhy.toml` manifest。
- 本地包目录布局。
- `hhy install <local-path>`。
- `hhy remove <package>`。
- `hhy list`。
- 包锁文件。
- Extension Protocol v1。
- 扩展进程启动、握手和关闭。
- 函数、类型、operator 和 action 注册。
- 分块 Stream 传输。
- 背压、取消和 timeout。
- Opaque handle 生命周期。
- capability 声明与用户授权。
- dry-run 和扩展 effect 集成。
- 扩展崩溃隔离与自动清理。

### 3.2 Manifest 草案

```toml
[package]
name = "example"
version = "1.0.0"
author = "Example Developer"
requires_hhy = ">=1.1,<2.0"

[extension]
kind = "process"
command = "hhy-example"
protocol = "1"

[capabilities]
read = []
write = []
network = []
process = false
```

### 3.3 协议能力

```text
handshake
register
call
call_result
stream_open
stream_item
stream_credit
stream_close
cancel
error
handle_release
shutdown
```

消息必须有 request_id、extension_id 和 protocol_version。Stream 使用 credit/window 机制实现背压，不能让扩展无限推送数据。

### 3.4 安装安全

- v1.1 只保证本地路径安装。
- manifest 和可执行文件计算完整性 Hash。
- 安装和列出包时展示作者、协议与 capability。
- v1.1 本地安装的 `author` 是署名，不是可信官方徽章；未来远程仓库必须通过签名验证发布者身份。
- 未经授权不能扩大文件、网络或进程权限。
- 扩展默认继承最小环境变量集合。
- 凭据通过显式 secret provider 传入，不复制完整宿主环境。
- 包更新改变 capability 时必须重新确认。

### 3.5 兼容承诺

- Extension Protocol 使用独立 major/minor 版本。
- 同一 major 内新增可忽略字段保持向后兼容。
- 未识别的必需能力导致握手失败。
- Runtime 不向扩展暴露 HhyValue 指针。
- v1.1 不承诺扩展进程跨 Runtime major 永久兼容。

### 3.6 v1.1 验收扩展

在 Office 之前先实现一个小型 reference extension：

```text
sample.echo(value) -> value
sample.range(n) -> Stream<Int>
sample.fail() -> Error
sample.wait(duration) -> cancellable value
```

它用于验证普通调用、Stream、错误、timeout、取消、崩溃隔离和 handle 清理。

## 4. v1.2：官方 Office 扩展验证协议

Office 是第一个复杂官方 Process Extension，用来证明协议可以承载真实办公工作负载，而不是把 Office 变成 HHY Core 的一部分。

### 4.1 第一阶段范围

```text
office.excel.open
office.excel.sheet
office.excel.rows
office.excel.new
office.excel.set_sheet
office.excel.save
```

第一阶段聚焦 `.xlsx` 的数据读取、转换和可靠输出。

### 4.2 Flow 使用

```hhy
import office.excel

let active = path("employees.xlsx")
    |> office.excel.open
    |> office.excel.sheet("Employees")
    |> office.excel.rows({ header: true })
    |> where { row -> row.Active == true }
    |> collect

office.excel.new()
    |> office.excel.set_sheet("Active Employees", active, {
        header: true,
        freeze_rows: 1,
        autofilter: true
    })
    |> office.excel.save(path("active-employees.xlsx"))
```

### 4.3 Office 验证目标

- Workbook Opaque handle 生命周期。
- Worksheet 到 `Stream<Map>` 的分块传输。
- 大数据背压。
- 文件读写 capability。
- dry-run 对 save action 的拦截。
- timeout 与用户取消。
- 扩展崩溃后的句柄失效和临时文件清理。
- DateTime、Percent、String、Number 与 null 的类型映射。
- 公式字符串不能由不可信文本隐式触发。
- 保存失败时保留旧文件。

### 4.4 Office 首版不做

- `.xls`。
- VBA 和宏执行。
- 密码破解。
- PivotTable、Power Query 和 Power Pivot。
- 完整公式计算引擎。
- 操控正在运行的 Excel GUI。
- Word 和 PowerPoint。

这些限制验证一个原则：扩展应解决明确问题，不应因为不在 Core 就无限扩张范围。

### 4.5 v1.2 验收条件

- Office 扩展不链接 HHY Runtime 内部符号。
- 同一个扩展包可在所有正式 HHY 平台运行。
- 读取十万行级工作表时 Stream 背压有效。
- 取消后扩展进程、Workbook handle 和临时文件全部清理。
- 扩展崩溃不会导致 HHY Runtime 崩溃。
- Office Error 能定位到扩展、工作簿、工作表和 Flow stage。
- 所有 effect 都经过 Runtime capability 与 dry-run 路径。

## 5. 以后：公开 Native ABI

Native ABI 不是必然目标。只有 Process Extension 无法满足经过测量的性能或系统集成需求时才开放。

### 5.1 开放前置条件

- HhyValue 内存模型至少经过一个 Runtime major 稳定周期。
- GC managed value、borrowed view 和显式资源所有权规则经过 sanitizer 与扩展压力测试。
- Stream operator ABI 已冻结。
- Error 与 Cancellation ABI 已冻结。
- effect 和 capability 调度不能被 Native Module 绕过。
- Runtime 能检测 ABI major 不匹配并拒绝加载。
- 官方至少有两个真实 Native Module 验证 ABI。
- 有清晰的崩溃、线程和安全责任声明。

### 5.2 Native ABI 最小表面

如果开放，只暴露 opaque C handle 和函数表，不公开 Runtime 结构体字段：

```text
hhy_runtime_api
hhy_value_handle
hhy_error_handle
hhy_cancel_handle
hhy_stream_vtable
hhy_module_descriptor
```

- 结构体携带 size 和 abi_version。
- 新字段只追加，不改变已有字段偏移。
- 字符串明确长度，不依赖 null termination。
- 所有函数明确 owned/borrowed。
- 不允许插件直接访问 GC 内部字段或持有未经注册的 managed pointer。
- Native Module 注册的 effect 仍经过 Runtime 调度。

### 5.3 不开放 ABI 的条件

如果 Process Extension 的性能、延迟和功能足够，HHY 可以长期不公开 Native ABI。减少崩溃面和兼容负担本身就是正确的工程选择。

## 6. 跨版本能力表

| 能力 | v1.0 | v1.1 | v1.2 | Later |
|---|---:|---:|---:|---:|
| 带点模块名 | ✓ | ✓ | ✓ | ✓ |
| 内部函数/operator Registry | ✓ | ✓ | ✓ | ✓ |
| Stream/Error/Cancel 内部 contract | ✓ | ✓ | ✓ | ✓ |
| EffectDispatcher | ✓ | ✓ | ✓ | ✓ |
| Pure HHY 本地包安装 | - | ✓ | ✓ | ✓ |
| Process Extension | - | ✓ | ✓ | ✓ |
| 本地包 manifest/lock | - | ✓ | ✓ | ✓ |
| 远程公共包仓库 | - | - | 可评估 | 可评估 |
| Office 扩展 | - | - | ✓ | ✓ |
| 公开 Native ABI | - | - | - | 条件满足后评估 |

## 7. 风险控制

### 不提前冻结协议

v1.0 只冻结内部逻辑 contract，不发布二进制或进程协议。这样可以通过真实实现修正消息格式，而不背负过早兼容承诺。

### 不用 Native ABI 验证第一套插件

Office 首版必须走 Process Extension。它能同时验证大数据、Opaque handle、文件权限、错误和取消，是比 Native ABI 更安全的压力测试。

### 不让扩展改变语言语法

扩展只能注册 qualified callable、类型和 Flow contract，不能注册关键字、运算符或 Parser 规则。这样安装包不会改变 `.hhy` 源码的基本含义。

### 不让扩展绕过 EffectDispatcher

扩展声明的读写、网络和进程行为必须经过 Runtime 授权。否则 dry-run、安全审计和未来沙箱都会失效。

### 不把 Package Version 当作 Protocol Version

包、HHY、Extension Protocol 和 Native ABI 分别版本化，避免升级一个 Office 功能却被误认为改变整个协议。

## 8. 当前结论

v1.0 目前已经预留的不是一个空泛的“插件接口”，而是一组可以在 Core 内部实现和测试的稳定边界：

```text
Qualified Namespace
Pipe Injection
Value Contract
Function/Operator Registry
Stream open/next/close
Error Contract
CancellationToken
EffectDispatcher
Resource Lifecycle
Checker Metadata
```

v1.0 不承诺外部扩展可运行。它承诺的是：当 v1.1 加入 Process Extension 时，不需要重写 HHY 的语言哲学和执行模型。
