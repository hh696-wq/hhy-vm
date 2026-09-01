# HHY Language v1.0 统一规范

> 当前语言规范：`1.0.0`（冻结）；当前兼容实现：`1.3.0-alpha`
> 规范状态：v1.0 已冻结
> 官网：[hhylang.dev](https://hhylang.dev)  
> 定位：Flow-first system scripting language  
> 口号：Pipe Everything.

本文档是 HHY v1.0 唯一规范来源。语法、运行时、标准库、CLI 和测试必须以本文档为准；实验实现与本文冲突时，应先修改规范并记录决策，再修改代码。

## 1. 产品定义

HHY 是一门以数据流为核心的系统脚本语言。它通过统一的管道模型连接文件、进程、网络与结构化数据，让系统自动化像描述数据流一样简单。

```text
HHY v1.0 = 完整动态脚本语言 + Flow 执行模型 + 系统标准库
```

```hhy
path("./logs")
    |> files("**/*.log")
    |> where { file -> file.size > 1mb }
    |> flat_map { file -> read_lines(file.path) }
    |> where { line -> contains(line, "ERROR") }
    |> take(100)
    |> save_lines(path("errors.txt"))
```

HHY 不是自然语言执行器，不依赖 AI，也不是把 Shell 命令换一套拼写。每份源码都有确定的 token、AST、作用域、执行计划和错误行为。

## 2. 语言原则

- **Flow**：文件、行、进程、HTTP 响应和系统事件采用统一 Stream 模型。
- **Pipe**：`|>` 是组合核心，不是附加语法糖。
- **System**：Path、File、Process、HttpRequest 和 FileEvent 是一等值。
- **Simple**：一套调用语法、显式闭包、动态类型和清晰错误。
- **Native Units**：Bytes、Duration 和 Percent 是原生值。
- **Lazy where useful**：Stream 惰性执行；普通标量表达式立即执行。
- **Explicit effects**：读取、写入、启动进程和发送网络请求具有明确算子。
- **Safe by default**：命令默认不经 Shell，TLS 默认验证，资源默认受限。

品牌表达：

```text
HHY
Pipe Everything.
A flow-first scripting language for system automation.
Built solo. Designed to flow.
```

## 3. v1.0 冻结决策

以下决策是实现前基线。若要更改，必须先修改本文档。

1. HHY 使用动态类型和运行时检查。
2. 变量默认不可变；可变绑定使用 `let mut`。
3. 普通调用统一使用 `name(args)`。
4. `value |> fn(a)` 等价于 `fn(value, a)`。
5. 无参数管道阶段允许 `value |> fn`，等价于 `fn(value)`。
6. `where` 等逐项算子只接受显式闭包，不使用隐式字段作用域。
7. 闭包完整形式为 `{ param -> expression }` 或 `{ param -> statements }`。
8. 单参数闭包可使用隐式 `it` 简写 `{ expression }`。
9. Map 使用 `{ key: value }`；代码块由语法上下文区分。
10. `map` 不自动展开嵌套 Stream；需要展开时使用 `flat_map`。
11. Stream 默认惰性、拉取式、单次消费。
12. 普通错误默认 fail-fast；单项错误必须显式转换为 Result 才能继续。
13. HTTP 先构造 HttpRequest，`send` 才产生副作用。
14. `run` 接受参数数组且不经过 Shell；Shell 模式必须显式使用 `shell`。
15. Parallel 默认保序、有限并发、有界缓冲、fail-fast。
16. C Runtime 使用受限的保守追踪 GC 管理语言堆；系统资源显式关闭，不能依赖 GC finalizer。
17. 跨 Parallel worker 的值采用冻结快照，不共享可变对象。
18. v1.0 预留扩展命名、类型和 Flow contract 边界，但不加载第三方扩展，也不冻结公开 Native ABI。
19. v1.0 正式支持 macOS arm64 与 Linux x86_64/arm64；Windows 为后续目标。

## 4. 源文件与词法规则

- 后缀为 `.hhy`。
- 源码为 UTF-8，接受 LF 与 CRLF。
- 标识符和关键字大小写敏感。
- 标识符使用 ASCII 字母、数字和 `_`，不能以数字开头。
- `{ ... }` 表示块或 Map，由所在语法位置确定。
- 换行默认结束语句；允许可选 `;`。
- 括号、方括号、花括号未闭合时换行不结束语句。
- 行尾或下一有效行开头出现 `|>` 时延续当前管道。
- `#` 是单行注释；首行允许 shebang。
- Tab 与空格不决定语义。
- String 字面量支持 `\\`、`\"`、`\n`、`\r`、`\t`、`\b`、`\f` 和 `\0`；其他反斜杠转义非法。Unicode 字符直接以 UTF-8 写入源码，v1.0 不定义 `\u` 源码转义（JSON 内的 `\uXXXX` 仍按 JSON 规范处理）。

```hhy
#!/usr/bin/env hhy

# first HHY program
let name = "HHY"
name |> print
```

### 4.1 Regex 与除法消歧

`/` 在期望表达式起点的位置开始 Regex；在已有左操作数后表示除法。

```hhy-snippet
let pattern = /ERROR|WARN/i
let ratio = total / count
```

Lexer 必须保存 token 行列和原始切片，Parser 错误必须展示源码片段。

## 5. 字面量与运算符

字面量：

```text
null true false
42 -10 0xff 0b1010 1.5 1e6
"hello" "Hello, {name}"
/ERROR|WARN/i
[1, 2, 3]
{ name: "Tom", age: 20 }
1..10
10mb 5s 80%
```

运算符优先级由高到低：

```text
() [] .
not  unary- unary+
* / %
+ -
< <= > >=
== !=
and
or
??
|>
=
```

规则：

- `and`、`or` 和 `??` 短路执行。
- `=` 只允许给 `let mut` 绑定赋值。
- `|>` 左结合。
- `%` 紧跟数字且无空格时属于 Percent；二元位置表示取模。
- Range `a..b` 包含起点、不包含终点。

## 6. 调用与 Pipe 语义

### 6.1 普通调用

所有普通函数与标准库调用使用括号：

```hhy
print("hello")
read_text(path("hello.txt"))
copy(path("a.txt"), path("backup/a.txt"))
```

不采用 `copy "a" to "b"` 之类的专用句式，避免 Parser 为每个标准库函数增加 grammar。

### 6.2 Pipe 降级规则

```text
x |> f           => f(x)
x |> f(a, b)     => f(x, a, b)
x |> obj.f(a)    => obj.f(x, a)
```

示例：

```hhy
"hello" |> upper |> print
path("./src") |> files("*.c") |> take(10) |> print
```

右侧必须是可调用值或调用表达式。不能把值自动注入任意子表达式。

### 6.3 Pipe 不自动完成的事情

- 不自动把单值转成 Stream。
- 不自动展开 `Stream<Stream<T>>`。
- 不自动访问 `it` 字段。
- 不自动忽略错误。
- 不自动字符串化结构化值。
- 不自动执行 Shell。

## 7. 动态值模型

核心类型：

```text
Null Bool Int Float String Regex BytesBuffer
List Map Range Function Error Result Stream
Bytes Duration Percent DateTime Path
File Directory FileEvent Process CommandResult
HttpRequest HttpResponse
```

`type()` 返回上述逻辑类型名；Result、File、Directory、FileEvent、Process 和 CommandResult 不得伪装成 Map。它们提供只读字段访问，并可在字段均可发送时跨 Parallel worker 复制；按照 JSON 规则，系统对象仍不能直接编码为 JSON。

### 7.1 Bool 与真值

条件必须是 Bool。HHY 不把空字符串、0、空 List 或 null 隐式当作 false。

```hhy-snippet
if value != null { ... }
```

### 7.2 数字

- Int 是有符号 64 位整数。
- Int 溢出产生 ValueError，不回绕、不静默转 Float。
- Int 与 Float 混合运算时 Int 转 Float。
- 除零产生 ValueError。
- Float 使用 IEEE 754 double。
- JSON 默认拒绝序列化 NaN 与 Infinity。
- `1 == 1.0` 为 true；其他类型不隐式转换后比较。

### 7.3 String

- String 是不可变 UTF-8 字节序列，创建时保证 UTF-8 有效。
- String 使用显式字节长度，不以 C 的 `\0` 作为结束语义；合法的 `U+0000` 必须在索引、比较、Map 键、JSON、文本算子和文件 I/O 中完整保留。
- `byte_length` 返回字节数。
- `length` 返回 Unicode code point 数量。
- v1.0 不承诺 grapheme cluster 索引。
- String 索引返回 Unicode code point 对应的单字符 String。
- `Bytes` 是带量纲的文件大小/内存大小数值；`BytesBuffer` 是不可变二进制
  数据。二进制内容必须使用 BytesBuffer，不放入 String，二者不得混用。
- Path、进程 argv/cwd/env、HTTP URL/proxy/header 等传给宿主 C API 的文本边界拒绝 `U+0000`，产生 ValueError；HTTP 与进程返回的文本必须通过 UTF-8 验证，任意二进制响应使用 BytesBuffer API。

### 7.4 相等与 Hash

- Null、Bool、数字、String、Units、Path 按值比较。
- List 和 Map 深度比较，并检测递归深度。
- Function、Stream、File、Process、Request 和 Response 不支持值相等。
- Map 键仅允许 String。
- `distinct/group` 只接受可 Hash 的标量键；不支持时产生 TypeError。

### 7.5 类型检查

```hhy-snippet
type(value)
is_type(value, "String")
```

HHY 不进行危险的 String/Number、String/Bool 隐式转换。

## 8. 变量、可变性与作用域

```hhy
let name = "HHY"
let mut count = 0
count = count + 1
```

- 使用块级词法作用域。
- 变量必须先声明后使用。
- 同一作用域禁止重复声明。
- 内层作用域允许遮蔽外层名称，Checker 给出 warning。
- `let` 绑定不可重新赋值。
- `let mut` 允许重新赋值。
- List 和 Map 默认不可变；更新操作返回新值。
- v1.0 不提供原地修改集合 API，减少引用循环和并发共享问题。
- 闭包捕获绑定当前值；捕获 `let mut` 时捕获共享 Cell，但该闭包不能发送到 Parallel worker。

## 9. 条件与循环

```hhy-snippet
if enabled {
    print("enabled")
} else if pending {
    print("pending")
} else {
    print("disabled")
}

for file in file_list {
    print(file.path)
}

let mut n = 0
while n < 10 {
    n = n + 1
}
```

v1.0 支持：

- `if / else if / else`。
- `for item in iterable`。
- `while condition`。
- `break` 与 `continue`。
- List、Map entries、Range 和 Stream 遍历。

`for` 消费 Stream。Stream 被消费后不能再次迭代。

## 10. 函数与闭包

```hhy
fn add(a, b) {
    return a + b
}
```

- 函数使用位置参数。
- 无显式 `return` 时返回 null。
- 支持递归、函数值和词法闭包。
- 参数数量在调用时检查。
- v1.0 不支持重载、泛型和默认参数。

完整闭包：

```hhy-snippet
users |> where { user -> user.active }
users |> map { user -> { name: user.name, email: user.email } }
```

隐式单参数简写：

```hhy-snippet
numbers |> map { it * 2 }
```

带多条语句的闭包必须显式声明参数并使用 `return`：

```hhy-snippet
numbers |> map { number ->
    let doubled = number * 2
    return doubled
}
```

## 11. Map 与代码块消歧

- `{ key: value }` 在表达式位置是 Map。
- 控制语句、函数和 catch 后的 `{ ... }` 是代码块。
- Flow 算子后紧随的 `{ param -> ... }` 是闭包。
- `{ it * 2 }` 只在明确需要闭包参数的位置允许作为隐式闭包。
- 空 `{}` 在表达式位置是空 Map；空块只出现在要求块的语法位置。

Parser 不根据换行或字段名猜测 Map/Block，而由父语法节点决定。

## 12. List、Map、Range 与 DateTime

List：

- `length`、`get`、`append`、`remove_at`、`contains`。
- 索引从 0 开始，不支持负索引。
- 越界产生 IndexError。
- 修改操作返回新 List。

Map：

- 键仅为 String，保持插入顺序且必须唯一；Map 字面量重复键由 Checker 拒绝，JSON Object 重复键产生 ValueError，不采用含糊的 first-wins/last-wins 规则。
- 键保存独立字节长度，因此允许合法 UTF-8 键包含 `U+0000`；查找、更新、删除、相等与 JSON 编解码均按完整字节序列处理。
- 支持 `map.key` 和 `map["key"]`。
- 普通缺失字段返回 null。
- 键存在性与值为 null 是不同状态；`require(map, key)` 仅在键缺失时产生 KeyError，存在且值为 null 时返回 null，`pick` 同样保留此类字段。
- `put/remove_key` 返回新 Map。

Range：

- `a..b` 包含 a，不包含 b。
- v1.0 Range 只接受 Int。
- Range 可直接迭代，不预先分配 List。

DateTime：

- 不提供易歧义的日期字面量。
- `datetime.parse(text, format, timezone)` 显式解析。
- `now()` 返回带时区的 DateTime。
- 序列化默认使用 RFC 3339。
- Duration 可以与 DateTime 加减。

## 13. Stream 模型

### 13.1 基本性质

- Stream 是惰性、拉取式、单次消费的数据序列。
- 下游调用 `next()` 时上游才产生下一项。
- 有限流、无限流和事件流使用相同接口。
- Stream 不支持复制、比较或 JSON 序列化。
- `collect()` 显式把有限 Stream 物化为 List。
- 消费完成、提前停止、错误或取消都会调用 `close()`。

### 13.2 Operator 生命周期

```text
open -> next* -> close
```

`close` 必须幂等，并从下游向上游传播。

### 13.3 Operator 分类

逐项流式：

```text
map flat_map where take skip inspect
```

有状态但有界：

```text
distinct debounce parallel
```

屏障或终端：

```text
sort group collect reduce count save
```

无限流进入 `sort/group/collect` 时必须要求显式边界，例如先使用 `take` 或时间窗口，否则产生 PlanError。

### 13.4 Map 与 Flat Map

```text
map       Stream<T> + (T -> U)         -> Stream<U>
flat_map  Stream<T> + (T -> Stream<U>) -> Stream<U>
```

`map` 永不自动展开返回的 List 或 Stream。

### 13.5 统一取消

Runtime 为每次执行创建 CancellationToken。Ctrl+C、timeout、未处理错误和显式 `cancel` 都触发同一取消路径。

所有文件读取、HTTP、watch、sleep、parallel 和子进程算子必须定期检查该 token。

## 14. Flow 标准算子

数据源：

```text
files read_lines processes watch stdin_lines range
```

转换：

```text
stream map flat_map for_each split trim replace lower upper
parse_json encode_json parse_csv encode_csv get pick
stdout_lines response_body response_bytes send_to
```

过滤：

```text
where contains starts_with ends_with regex_match distinct
```

集合与聚合：

```text
take skip first last count sort_by group_by reduce
sum min max any all collect
```

动作：

```text
print print_error save_text save_lines write_bytes copy move remove send run
```

控制：

```text
parallel retry timeout on_error attempt debounce
```

标准库名称使用 snake_case。每个算子必须在标准库参考中记录：输入、输出、是否惰性、是否产生副作用、错误类型和资源上限。

通用标量与集合函数：

```text
length type is_type to_int to_float
path path_join get require put remove_key
```

- `group_by` 产生 `Stream<Group>`，Group 包含 `key` 与 `values`；`values` 是有限 List。
- `length` 接受 String、List 或 Map；Stream 使用终端算子 `count`。
- `to_int/to_float` 是显式转换，转换失败产生 ValueError。
- `stream` 接受 List、Map entries 或 Range，返回单次消费 Stream。
- `reduce(initial) { state -> ... }` 是终端算子；`state` 是包含 `acc`、`item`
  和从 0 开始的 `index` 的不可变 Map。闭包返回值成为下一项的 `acc`。
- `print(Stream)` / `print_error(Stream)` 逐项拉取并每项输出一行，不物化整个流；要以 List 形式输出时显式使用 `collect |> print`。

## 15. 原生单位

```text
Bytes     b kb mb gb tb kib mib gib tib
Duration  ns us ms s min h d
Percent   %
```

- `kb/mb/gb` 是十进制，`kib/mib/gib` 是二进制。
- 内部使用固定基础单位和溢出检查。
- 同量纲可比较和加减。
- Bytes 和 Duration 不可比较。
- `Percent` 内部以 double 表示，不强制限制在 0% 到 100%。
- `sleep(500ms)`、`timeout(5s)` 和 File.size 共用同一单位类型。

## 16. Path、文件与目录

String 不隐式变成 Path。使用 `path(text)` 构造并规范化 Path。

```hhy
path("./src")
    |> files("**/*.c", { follow_symlinks: false })
    |> where { file -> file.size > 10kb }
    |> sort_by({ order: "desc" }) { file -> file.size }
    |> print
```

File 字段：

```text
path name extension size created modified
is_file is_dir is_symlink
```

`files(pattern, options)` 对匹配的普通文件/符号链接产生 File，对匹配目录产生 Directory；遍历根目录本身不作为结果。两者共享上述只读字段。

`modified` 为 DateTime；`created` 为 `DateTime | Null`。当操作系统或文件系统不提供可靠的创建时间时必须返回 Null，不得用修改时间伪造。

基础 API：

```hhy
read_text(path("hello.txt"))
read_lines(path("app.log"))
read_bytes(path("image.png"))
write_text(path("hello.txt"), "Hello HHY")
append_text(path("app.log"), "done\n")
copy(path("a.txt"), path("backup/a.txt"))
move(path("a.txt"), path("data/a.txt"))
remove(path("temp.txt"))
```

规则：

- 支持 `*`、`?`、`**` glob。
- 相对 Path 基于进程启动工作目录，不随 import 文件改变。
- `path(text)` 做词法规范化：折叠重复分隔符和 `.`，解析可消除的 `..`，但不访问文件系统、不解析符号链接。
- Path 提供只读成员 `name`、`extension` 和 `parent`；`extension` 包含前导 `.`，无扩展名时为空字符串。
- 默认不跟随目录符号链接；`files(..., { follow_symlinks: true })` 可开启，并跳过检测到的目录循环。
- 文本默认 UTF-8；无效 UTF-8 产生 EncodingError。
- `read_lines` 流式读取并移除行终止符。
- `save_lines` 在每个元素后写平台无关的 `\n`。
- `save_text` 只接受 String 或 Stream<String>，不隐式编码 Map/List。
- `save_text(Stream)` 与 `save_lines(Stream)` 直接拉取上游并写入临时文件，不先
  `collect` 或拼接整个输出，因此内存占用不随输出文件大小线性增长。
- `write_text/save_*` 默认原子替换；同目录临时文件写完后 rename。
- `write_text`、`write_bytes`、`save_text`、`save_lines` 接受可选 options Map：
  `overwrite` 默认 `true`，`create_parents` 默认 `false`。目标已存在且
  `overwrite: false` 时使用平台原子 no-replace rename 提交，目标已存在或并发创建时产生 IoError，不允许 check-then-rename 竞争覆盖；`create_parents: true` 逐级创建缺失目录。
- `--dry-run` 拦截 copy、move、remove、write 和 save。

## 17. 文本与 Regex

```hhy
read_lines(path("app.log"))
    |> where { line -> contains(line, "ERROR") }
    |> map { line -> replace(line, "ERROR", "ERR") }
    |> print
```

v1.0 提供：

```text
split join trim trim_start trim_end
contains starts_with ends_with replace
lower upper regex_match regex_captures
```

- 文本函数按 Unicode code point 工作。
- Regex 字面量支持 `i`、`m`、`s`、`u` flags。
- Regex 引擎必须提供执行限制或保证线性时间，防止资源耗尽。
- `regex_captures` 返回完整匹配及字节位置；编号捕获位于 `groups: List`，命名捕获位于 `named: Map`。
- v1.0 使用 PCRE2 8-bit 后端，并设置 match、depth、heap、输入大小、pattern 大小和捕获组数量上限。

## 18. JSON 与 CSV

JSON：

```hhy
read_text(path("users.json"))
    |> parse_json
    |> get("users")
    |> where { user -> user.active }
    |> map { user -> { name: user.name, email: user.email } }
    |> encode_json({ pretty: true })
    |> print
```

- Object 映射 Map，Array 映射 List。
- JSON Number 根据表示映射 Int 或 Float。
- JSON 解析错误包含输入行列。
- 最大大小、嵌套深度和字符串长度受资源配置限制。
- NaN、Infinity、Function、Stream 和系统对象不能编码为 JSON。

CSV：

- `parse_csv` 接受 String 或 Stream<String>，返回 `Stream<Map>`。
- `encode_csv` 接受 `Stream<Map>`，返回 `Stream<String>`。
- 支持 `header: Bool`、单字符 `delimiter` 和单字符 `quote` 选项。
- `encode_csv` 的每个流元素是一条不带行终止符的 CSV record；由 `save_lines` 统一写入 `\n`。因此 v1.0 不提供容易造成双换行的 `newline` 编码选项。
- 保持流式，不要求加载整个文件。

## 19. 进程与命令

```hhy
processes
    |> where { process -> process.cpu > 50% }
    |> sort_by({ order: "desc" }) { process -> process.cpu }
    |> take(10)
    |> print
```

Process 字段：

```text
pid name cpu memory status command
```

命令执行：

```hhy
run(["git", "status"])

run(["git", "log", "--oneline"])
    |> stdout_lines
    |> take(10)
    |> print
```

`run(args, options)` 返回 CommandResult：

```text
exit_code stdout stderr duration
```

- 默认等待命令完成并受输出大小限制。
- `stdout_lines` 从 CommandResult.stdout 产生 Stream<String>。
- options 支持 cwd、env、stdin、timeout 和最大输出。
- timeout/Ctrl+C 先发送优雅终止，宽限期后强制终止。
- `shell(command)` 是显式危险 API，并在 Checker 中产生提示。
- `--dry-run` 不执行 run 或 shell，只输出计划。

`sort_by(options) { item -> key }` 的 `order` 只能是 `"asc"` 或 `"desc"`，默认 `"asc"`。排序必须稳定；键不可比较时产生 TypeError。

## 20. 环境、参数与系统信息

```hhy
env.PATH
env["HOME"]
args

system.os
system.arch
system.hostname
system.cpu
system.memory
system.cwd
system.temp
```

- `args` 是不含脚本路径的 `List<String>`。
- CLI 参数和宿主环境变量进入 String/Map 前必须通过 UTF-8 验证；无效宿主字节产生 EncodingError，不得制造违反 String 不变量的值。
- `exit(code)` 立即结束脚本并执行资源清理。
- 缺失环境变量返回 null；`require_env(name)` 在缺失时产生 KeyError。
- 环境覆盖只传给子进程，不修改父进程环境。
- `stdin_lines` 是单次消费的 Stream<String>。

## 21. HTTP 请求计划

HTTP 使用显式 Request -> Policy -> Send -> Response 模型：

```hhy
http.get("https://api.example.com/users")
    |> timeout(5s)
    |> retry({ count: 3, backoff: 200ms })
    |> send
    |> response_body
    |> parse_json
    |> get("users")
    |> where { user -> user.active }
    |> print
```

语义：

- `http.get/post/put/delete` 只构造 HttpRequest，不发送网络请求。
- `timeout` 和 `retry` 修改请求执行计划。
- `send(HttpRequest)` 是真正的网络副作用，返回 HttpResponse。
- `send_to(HttpRequest, Path)` 将响应体直接流式写入同目录临时文件，成功后原子发布；返回的 HttpResponse 提供 `path` 和 `size`，不持有完整 body。
- `response_body` 验证状态并返回 String；非成功状态默认产生 HttpStatusError。
- `response_bytes` 返回 BytesBuffer。
- retry 默认只作用于连接错误、timeout、429 和部分 5xx。
- GET/PUT/DELETE 可按策略重试；POST 默认不自动重试。
- TLS 验证默认开启。
- 支持 query、headers、body、代理、重定向和响应大小限制。
- Authorization、Cookie 等敏感 header 不进入普通日志或 Error 展示。
- `--dry-run` 不发送请求，只输出方法、脱敏 URL 与策略。

## 22. Watch 与定时

```hhy
watch(path("./src"), { recursive: true })
    |> where { event -> event.path.extension == ".c" }
    |> debounce(200ms)
    |> for_each { event -> run(["make"]) }
```

FileEvent：

```text
kind path old_path timestamp
```

- kind 为 created、modified、removed 或 renamed。
- old_path 仅 renamed 时存在。
- macOS 使用 kqueue、Linux 使用 inotify；平台事件只负责唤醒，Runtime 通过快照差异归一化字段与语义。
- 递归 watcher 注册子目录并受 `max_open_files` 限制；运行中新增目录会刷新注册集合。
- HHY 原子写事务产生的 `.hhy-tmp-*` 内部文件不会泄漏为用户 FileEvent。
- rename 通过稳定文件标识配对为单个 `renamed` 事件并填写 `old_path`；短时间重复事件可由 debounce 合并。
- `debounce(window)` 使用 leading-edge 语义：同一标量值，或同一 `kind + path`
  的 FileEvent，首项立即输出；窗口内后续重复项被合并，并从最后一次重复重新计时。
  不同事件键互不阻塞。该定义避免无限事件流产生隐藏并发或无界尾项缓冲。
- Watch 是无限 Stream，必须通过取消、timeout 或进程结束关闭。

定时：

```hhy
every(5s)
    |> flat_map { tick -> processes }
    |> where { process -> process.cpu > 80% }
    |> print
```

`every` 返回无限 Stream<Tick>。如果下游仍在处理，默认背压，不重叠执行同一项。

## 23. Parallel

```hhy-snippet
urls
    |> parallel(8) { url ->
        http.get(url)
            |> timeout(5s)
            |> send
            |> response_body
    }
    |> print
```

规则：

- 参数是最大并发任务数，必须大于 0 且受全局上限约束。
- 返回 `Stream<U>`，语义等同并发 map，不自动 flatten。
- 默认按输入顺序输出。
- 使用有界输入队列与有界结果缓冲区。
- 队首任务缓慢时会产生保序阻塞，这是明确语义。
- 默认 fail-fast；首个错误停止拉取新输入并取消未完成任务。
- worker 接收输入值与闭包捕获值的冻结快照。
- 捕获 `let mut` Cell、Stream、File handle、Request body stream 等不可发送值时产生 CheckError。
- Runtime 内部可用线程池，但不向语言公开线程、锁或 async/await。
- `--dry-run` 下 `parallel` 不创建 worker 进程；Runtime 以惰性、保序的顺序
  map 执行闭包，从而继续展示闭包内 effect plan，同时保证计划检查本身没有
  进程副作用。

需要展平并发返回流时显式使用：

```hhy-snippet
items
    |> parallel(4) { item -> process(item) }
    |> flat_map { stream -> stream }
```

## 24. 错误模型

Error 字段：

```text
kind code message source stage cause stack context
```

错误类别至少包括：

```text
SyntaxError CheckError TypeError ValueError
IndexError KeyError EncodingError
IoError ProcessError HttpError HttpStatusError
TimeoutError CancelledError ResourceLimitError PlanError
```

### 24.1 默认传播

- 普通函数错误立即向调用者传播。
- Stream 某一项失败默认终止整条 Stream。
- 未处理错误使脚本以非零状态退出。
- Error context 默认不包含完整文件内容、凭据或 HTTP 敏感 header。

### 24.2 Try/Catch

```hhy
try {
    let data = read_text(path("config.json")) |> parse_json
    print(data)
} catch err {
    err |> print_error
}
```

`catch` 捕获整个 try 块传播出的第一个错误。catch 正常结束后程序继续；`throw(err)` 可重新抛出。

### 24.3 Flow on_error

```hhy-snippet
pipeline
    |> on_error { err ->
        print_error(err)
        return fallback_stream
    }
```

`on_error` 处理整条上游失败。处理器必须返回与上游相同类别的替代值，或重新抛出；不会隐式跳过错误项。

### 24.4 单项错误

`attempt` 把每项执行结果转换为 Result：

```hhy-snippet
files
    |> map { file -> attempt { read_text(file.path) } }
    |> where { result -> result.ok }
    |> map { result -> result.value }
```

Result 字段为 `ok`、`value` 和 `error`。这样继续处理错误项是显式行为。

`attempt { ... }` 是语言级表达式：执行块并捕获传播出的错误。成功时 Result.value 是块中最后一个表达式的值；失败时 Result.error 保存错误。空成功块的 value 为 null。

### 24.5 资源清理

- 每个 Stream operator 的 close 必须幂等。
- Error、return、exit、timeout 和 Ctrl+C 走同一 unwind/close 路径。
- 原子文件输出在失败时删除临时文件且保留旧文件。
- 子进程、HTTP response、watcher 和 worker 都必须响应取消。

## 25. 模块系统

模块语法：

```hhy-snippet
import "./lib/report.hhy" as report
import { parse, validate } from "./lib/data.hhy"
import http

export let version = "1.0"

export fn build_report(data) {
    return report.build(data)
}
```

规则：

- 相对 import 基于当前源码文件目录。
- 标准库模块使用裸名称，本地模块使用相对或绝对 Path。
- 标准库名称优先于同名当前目录文件；本地文件必须显式相对路径。
- 模块拥有独立顶层作用域。
- 模块在首次 import 时执行一次并缓存。
- 循环 import 在执行前产生 CheckError。
- 只有 `export` 名称对外可见。
- 模块顶层可以读取 env，但不能读取调用方局部变量。
- `args` 在所有模块中只读可见。
- v1.0 不包含远程 import 和公共包仓库。

## 26. 扩展架构与未来兼容边界

HHY 核心负责 Flow、值模型、错误、取消和资源限制；扩展负责提供可以进入 Flow 的新数据源、类型和动作。

```text
HHY Core
  + Extension Types
  + Extension Sources
  + Extension Operators
  + Extension Actions
  = Same Flow Model
```

例如未来的 Office 扩展不需要修改 Pipe 语义：

```hhy-snippet
import office.excel

path("employees.xlsx")
    |> office.excel.open
    |> office.excel.sheet("Employees")
    |> office.excel.rows({ header: true })
    |> where { row -> row.Active == true }
    |> collect
```

Workbook 和 Worksheet 由扩展提供；`where`、`collect`、Error、timeout 和取消仍由 HHY Core 提供。

### 26.1 三层扩展模型

#### Pure HHY Package

由 `.hhy` 模块组成，只组合语言和已授权标准库能力。它跨平台、不接触 Runtime 内存布局，适合文本处理、数据转换、校验规则和可复用 Flow。

```text
text-utils/
├── hhy.toml
└── src/main.hhy
```

#### Process Extension

扩展作为独立进程运行，通过版本化协议与 HHY Runtime 通信。这是未来第三方扩展的首选方式。

```text
HHY Runtime <-> Extension Protocol <-> office-hhy process
```

- 扩展崩溃不直接破坏 Runtime 内存。
- 可使用 C、C++、Rust、Go 等实现。
- Runtime 可以施加 timeout、取消、进程和内存限制。
- 协议独立于 HHY 内部 C ABI 演进。
- 适合 Office、数据库、云服务和大型第三方集成。

#### Native Module

Native Module 直接加载到 HHY 进程，只适合官方、可信且对性能敏感的底层组件。

- 性能和调用延迟最好。
- 插件崩溃会导致 Runtime 崩溃。
- 插件拥有宿主进程权限。
- 内存所有权和 ABI 兼容要求最高。
- v1.0 不开放第三方 Native Module ABI。

### 26.2 扩展注册模型

扩展只能注册普通函数或 Flow contract，不能引入一套与 HHY 不同的执行模型。

Office 扩展的逻辑签名示例：

```text
office.excel.open
Path -> Workbook

office.excel.sheet
Workbook, String -> Worksheet

office.excel.rows
Worksheet, Map -> Stream<Map>

office.excel.set_sheet
Workbook, String, List<Map>, Map -> Workbook

office.excel.save
Workbook, Path -> Null
```

每个注册项必须声明：

```text
qualified_name
kind: function | source | operator | action
input_contract
output_contract
lazy: true | false
effect: none | filesystem | process | network | custom
cancel: supported | unsupported
threading: main | worker | isolated_process
protocol_version
```

即使 HHY 使用动态类型，Runtime 仍使用 contract 做调用前检查、执行计划展示和错误诊断。

### 26.3 Process Extension 协议原则

v1.0 只预留边界，不实现协议。后续协议必须满足：

- 拥有独立协议版本，不复用 HHY Runtime 版本号。
- 请求和响应具有稳定 ID。
- 普通值使用确定性结构化编码。
- 大 Bytes 和 Stream 使用分块传输。
- 支持背压、取消、timeout 和扩展主动错误。
- Error 转换成 HHY Error，并保留扩展名称和 stage。
- 扩展异常退出转换成 ExtensionCrashedError。
- 句柄由 Runtime 管理生命周期，进程退出时全部失效。
- 协议不得传递裸指针或宿主内存地址。

### 26.4 扩展类型与资源

扩展类型分为：

- **Serializable value**：可复制、可编码，例如普通记录和 Office Cell 值。
- **Opaque handle**：只在扩展内部存在，例如 Workbook 句柄。

Opaque handle：

- 包含 extension_id、handle_id 和 generation。
- 不支持相等、Hash、JSON 编码或跨 Parallel worker 共享。
- 使用后由 Runtime 或显式 close 释放。
- 扩展重启后旧 handle 失效。
- Pure HHY Package 不能伪造 handle。

扩展返回 Stream 时，Runtime 使用统一 open/next/close contract 包装，因此下游仍获得 HHY Stream 的背压和取消行为。

### 26.5 能力与权限

未来扩展 manifest 必须声明能力：

```toml
[package]
name = "office"
version = "1.0.0"
author = "HHY Official"

[extension]
kind = "process"
command = "hhy-office"
protocol = "1"

[capabilities]
read = ["*.xlsx"]
write = ["*.xlsx"]
network = []
```

- 安装扩展不等于永久授予全部权限。
- `author` 是必须展示的作者署名；本地安装不把该字符串当作签名或身份凭证。
- Runtime 根据脚本输入和 CLI 授权限制文件、进程与网络范围。
- dry-run 必须展示扩展声明的 action。
- 未声明或未授权的 effect 在执行前产生 PermissionError。

完整 capability 沙箱不是 v1.0 功能，但核心 API 必须经过统一 effect 调度点，避免未来无法加入权限控制。

### 26.6 包与版本

未来包清单使用 `hhy.toml`，分别版本化：

```text
package_version
required_hhy_version
extension_protocol_version
native_abi_version
```

- Pure HHY Package 遵循 HHY 语言与标准库兼容政策。
- Process Extension 只依赖 Extension Protocol，不依赖 HhyValue 内存布局。
- Native Module 必须精确匹配 Native ABI major version。
- 包锁文件记录精确版本和完整性 Hash。
- 远程安装必须验证 Hash 和签名；v1.0 不实现远程安装。

### 26.7 v1.0 实际承诺

v1.0 只承诺：

- 语法支持带点的模块限定名，例如 `office.excel`。
- 标准库不会占用任意第三方顶级命名空间。
- HhyValue、Stream、Error、CancellationToken 和 effect 调度保持清晰内部边界。
- 标准库算子使用与未来扩展相同的逻辑 contract 描述。
- Checker 对未安装模块产生 ModuleNotFoundError，而不是语法错误。

v1.0 不承诺：

- 包管理器或公共包仓库。
- 第三方扩展安装或加载。
- Process Extension Protocol 的稳定格式。
- 公开 Native C ABI。
- Office 扩展本身。

建议路线：

```text
v1.0  Core contracts and namespace reservation
v1.1  Process Extension Protocol + local package install
v1.2  official HTML extension as protocol validation
later public Native ABI after Runtime stabilization
```

详细里程碑、兼容承诺与验收条件见 [`EXTENSION_ROADMAP.md`](EXTENSION_ROADMAP.md)。本文档继续负责语言语义和核心 contract；路线图不重复定义语义。

## 27. CLI

```text
hhy script.hhy [args...]
hhy run script.hhy [args...]
hhy repl
hhy fmt script.hhy
hhy fmt --check
hhy check script.hhy
hhy bytecode script.hhy
hhy run --dry-run script.hhy
hhy profile [--cpu|--heap] [--format text|json] [--output path] script.hhy [args...]
hhy --version
hhy --help
```

- `hhy script.hhy` 是 `hhy run script.hhy` 的简写。
- `--` 后所有参数原样放入 `args`。
- `repl` 支持表达式、块和多行 Pipe。
- `fmt` 是唯一官方格式，必须幂等并保留注释。
- `check` 检查语法、作用域、模块、不可发送捕获和已知标准库调用，不承诺完整静态类型检查。
- 非交互输出稳定，支持 `NO_COLOR`。
- `--dry-run` 拦截文件写动作、进程启动和 HTTP send。
- `profile` 执行脚本并报告逻辑函数和 builtin 的 CPU 样本、调用次数与托管 Heap
  分配；报告默认写入 stderr，脚本 stdout 和退出码保持运行语义。
- `profile` 默认同时采集 CPU 和 Heap；单独指定 `--cpu` 或 `--heap` 时只采集选中
  类别。`--format json` 提供机器可读报告，`--output` 将报告写入文件。
- `bytecode` 是 v1.3.0-alpha 的实验性编译、验证和反汇编入口；它输出内存内部
  Bytecode IR，不执行 Bytecode、不承诺 opcode 或磁盘格式兼容，也不改变 `run`
  默认使用 AST evaluator 的行为。

退出码：

```text
0 success
1 unhandled runtime error
2 syntax/check error
3 invalid CLI usage
4 IO/process/network error
5 timeout/cancel
```

## 28. 安全与资源限制

每次执行创建 RuntimeLimits：

```text
max_memory
max_open_files
max_processes
max_parallelism
max_http_body
max_regex_steps
max_recursion
max_runtime
```

默认值：`max_memory=512mib`、`max_open_files=256`、`max_processes=16`、
`max_parallelism=16`、`max_http_body=16mib`、`max_regex_steps=1000000`、
`max_recursion=256`；`max_runtime=0` 表示普通 CLI 脚本默认不设置总时限。
嵌入方通过 `HhyRuntimeLimits` 传入覆盖值；CLI 使用可重复的
`--limit NAME=VALUE`，例如：

```sh
hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
```

大小值必须带 `b/kb/mb/gb/kib/mib/gib`，时间值必须带
`ns/us/ms/s/min/h`，计数值不带单位。非法或零限制是 CLI usage error。

默认行为：

- run 不经 Shell。
- TLS 验证开启。
- 敏感 header 和环境值脱敏。
- 文件递归不跟随 symlink。
- Regex 受限。
- 临时文件安全创建。
- Ctrl+C 先优雅取消，再按宽限期强制清理。
- dry-run 不产生外部副作用。

`max_memory` 是每次 Runtime 执行相对启动基线计算的语言堆上限。接近上限时
Runtime 先触发一次完整 GC；回收后仍无法满足分配时产生
`ResourceLimitError(HHY_MEMORY_LIMIT)`，走正常 Stream close 与资源 unwind，
不得调用 `abort` 或以内部错误码直接终止宿主进程。

v1.0 不实现完整 capability 沙箱，但 API 设计不得绕开未来的权限检查点。

## 29. C Runtime 与内存所有权

### 28.1 HhyValue

HhyValue 使用 tagged union 表示标量；String、List、Map、Function、Error 和系统对象指向受 GC 管理的对象。

```text
scalar: Null Bool Int Float Bytes Duration Percent
heap:   String Regex List Map Function Error Result Stream system objects
```

### 28.2 所有权规则

- v1.0 内部语言堆使用 Boehm–Demers–Weiser conservative GC；公开 Native ABI 尚未冻结。
- Runtime C API 仍明确区分 managed value、borrowed view 与显式系统资源，不能把三者混用。
- managed value 可由环境、集合、闭包和 Stream 持有；失去可达性后由 GC 回收。
- String、List 和 Map 逻辑不可变，可安全共享。
- 更新集合返回新对象，内部可以 copy-on-write。
- HHY v1.0 没有原地集合更新，因此用户语法不能直接构造自引用集合；GC 能安全处理内部引用环。
- 闭包捕获不可变值；可变 Cell 不允许进入跨线程 worker。
- 文件描述符、进程、HTTP handle、watcher 和 worker 不依赖 GC finalizer，统一通过 execution unwind 和 operator close 显式释放。

### 28.3 Parallel 隔离

- 可冻结的不可变值通过版本化二进制快照传给隔离 worker，不共享 GC heap 指针。
- 可变 Cell、Stream 和打开的资源句柄不可发送。
- worker 返回值在进入主执行流前转换为可共享不可变值。
- v1.0 worker 使用隔离进程；主 Runtime 与 worker 各自拥有 GC heap，结果反序列化后进入主 heap。

### 28.4 运行时架构

```text
Source
  -> Lexer
  -> Parser
  -> AST
  -> Resolver / Checker
  -> Execution Plan
  -> Runtime / Flow
  -> Platform / IO / HTTP
```

建议目录：

```text
include/hhy/   内部公共接口
src/lexer/     词法分析
src/parser/    语法分析
src/ast/       AST 与打印器
src/check/     名称与基础语义检查
src/runtime/   Value、作用域、函数、Error、取消
src/flow/      Stream 与 operator
src/stdlib/    文件、文本、JSON、CSV、系统、HTTP
src/platform/  POSIX 平台适配
src/cli/       run、repl、fmt、check
tests/         单元、集成、端到端和模糊测试
```

v1.0 使用 AST 解释器；字节码 VM、JIT 和 LLVM 不属于 v1.0。

## 30. 第三方依赖策略

C 实现不应从零编写高风险协议与解析器。选型阶段必须评估许可证、维护状态、跨平台和静态链接能力。

固定边界：

- HTTP/TLS 使用成熟库，不自研 TLS。
- JSON 使用核心内受测的有界严格解析器与编码器。
- Regex 使用 PCRE2 8-bit，并配置执行资源上限。
- 文件 watch 使用平台原生 API，并包在统一 platform 接口后。
- Unicode 至少使用经过验证的 UTF-8 库或集中实现，不让各模块自行处理。

依赖版本、许可证和发布链接信息记录在 `DEPENDENCIES.md`。

## 31. 跨平台

v1.0 正式支持：

- macOS arm64。
- Linux x86_64。
- Linux arm64。

Windows 为 v1.1 候选，不属于 v1.0 发布阻塞项。

platform 层统一：

- Path 分隔符与规范化。
- 文件元数据。
- watch 事件。
- 进程枚举与终止。
- signal 与取消。
- 换行与终端能力。
- 可执行文件查找。

无法提供的系统字段返回 null，不伪造数值；标准库文档列出平台差异。

## 32. 开发阶段与冻结点

### v0.1：Grammar Freeze

- Lexer、正式 EBNF、Parser、AST 和 AST printer。
- 字面量、表达式、变量、块、函数、闭包、Pipe。
- 通过语法快照测试后冻结核心 grammar。

### v0.2：Runtime Freeze

- HhyValue、追踪 GC、不可变集合、作用域、函数、Error、Result。
- 数值、Unicode、相等和所有权测试。
- 冻结 Value ABI 与内存规则。

### v0.3：Flow Freeze

- Stream、open/next/close、取消、map、flat_map、where、take、reduce、print。
- 有限流、大数据流和提前终止测试。
- 冻结 Pipe 与 operator contract。

### v0.4：Core Preview

- Path、files、read_lines、文本、Regex、JSON、CSV、Units、模块。
- 可完成真实日志与数据处理脚本。
- Core Preview 后才并行扩展系统能力。

### v0.5：System

- processes、run、env、system 和资源限制。

### v0.6：Network

- HttpRequest、policy、send、HttpResponse、TLS、retry 和 timeout。

### v0.7：Automation

- watch、every、parallel、debounce 和统一取消。

### v0.8：Tooling

- REPL、fmt、check、错误诊断和文档工具。

### v0.9：Release Hardening

- 性能、跨平台、安装包、安全审计、模糊测试和文档 CI。

### v1.0：Stable

- 冻结 grammar、模块、错误模型、核心标准库和 CLI。
- 发布兼容政策：v1.x 可新增非关键字 API，但不破坏合法 v1.0 程序语义。

## 33. 测试要求

- Lexer token 与错误位置测试。
- Parser AST 快照和错误恢复测试。
- Formatter 幂等与注释保留测试。
- Value、GC 压力、集合不可变和资源所有权测试。
- Unicode、数字溢出、单位和 Regex 测试。
- Stream 惰性、背压、提前终止和 close 测试。
- 无限流进入屏障的 PlanError 测试。
- 文件、JSON、CSV、进程和 HTTP 集成测试。
- retry、timeout、cancel 和 atomic save 故障注入测试。
- watch 平台归一化测试。
- parallel 保序、缓冲、错误和取消测试。
- AddressSanitizer 与 UndefinedBehaviorSanitizer。
- Release 与 Debug/Sanitizer 使用隔离对象目录；`make test-debug` 不得与 Release
  对象混合链接。
- Lexer、Parser、JSON、CSV、Regex 边界模糊测试。
- `make fuzz-smoke` 提供快速确定性回归；`make fuzz-ci` 必须使用 libFuzzer + ASan/UBSan 做覆盖引导运行，二者不能互相冒充。
- 文档中标为完整 `hhy` 的代码块由 CI 运行 Parser + Checker；依赖文件、进程、HTTP 或 watcher 的完整场景必须由对应本地 acceptance 脚本真实执行，不能只做语法检查。仅用于说明局部语法且故意省略上下文的代码块必须标为 `hhy-snippet`。

## 34. v1.0 明确不做

- 数据库与 ORM。
- Web Framework、HTTP Server 和 WebSocket。
- GUI。
- AI、机器学习和自然语言执行。
- async/await、Actor 和公开线程 API。
- 可变共享内存并发。
- 复杂静态类型、泛型、trait 和宏。
- JIT、LLVM、本地编译和字节码 VM。
- 远程 import 和官方包仓库。
- 第三方扩展安装、Process Extension Protocol 和公开 Native C ABI。
- Office 扩展本身；它是扩展体系稳定后的独立项目。
- Windows 正式支持。

## 35. 核心验收程序

以下五类程序的可重复本地版本位于 `tests/acceptance/`。CI 使用大日志夹具、
本地 HTTP server、真实进程枚举和原生文件 watcher 执行它们；不得只做语法检查。

### 35.1 文件、文本、单位与并发

```hhy
path("./logs")
    |> files("**/*.log")
    |> where { file -> file.size > 1mb }
    |> parallel(4) { file ->
        read_lines(file.path)
            |> where { line -> contains(line, "ERROR") }
            |> collect
    }
    |> flat_map { lines -> lines |> stream }
    |> save_lines(path("errors.txt"))
```

### 35.2 HTTP、JSON 与错误

```hhy
http.get("https://api.example.com/users")
    |> timeout(5s)
    |> retry({ count: 3, backoff: 200ms })
    |> send
    |> response_body
    |> parse_json
    |> get("users")
    |> where { user -> user.active }
    |> map { user -> { name: user.name, email: user.email } }
    |> encode_json({ pretty: true })
    |> save_text(path("active-users.json"))
    |> on_error { err ->
        print_error(err)
        throw(err)
    }
```

### 35.3 进程与系统信息

```hhy
processes
    |> where { process ->
        process.memory > 1gb or process.cpu > 80%
    }
    |> sort_by({ order: "desc" }) { process -> process.memory }
    |> take(10)
    |> print
```

### 35.4 Watch 自动化

```hhy
watch(path("./src"), { recursive: true })
    |> where { event -> event.path.extension == ".c" }
    |> debounce(200ms)
    |> for_each { event ->
        run(["make"], { timeout: 2min })
    }
```

### 35.5 普通语言能力

```hhy-snippet
import { normalize } from "./lib/data.hhy"

fn summarize(items) {
    let mut total = 0

    for item in items {
        if item.enabled {
            total = total + item.score
        }
    }

    return {
        count: items |> stream |> count,
        total: total
    }
}

let result = summarize([
    { enabled: true, score: 10 },
    { enabled: false, score: 20 },
    { enabled: true, score: 30 }
])

result |> normalize |> encode_json |> print
```

## 36. 发布条件

HHY v1.0 必须同时满足：

- 正式 EBNF、优先级和换行规则已冻结。
- Pipe 注入、闭包、Map/Block 消歧和 Stream contract 已冻结。
- Value 所有权、GC 边界、不可发送值和资源释放规则已冻结。
- 模块限定名、内部 operator contract 和 effect 调度点可承载未来扩展，但 v1.0 不加载第三方扩展。
- 五类核心验收脚本真实执行成功。
- 有限流、大文件流和无限事件流行为明确。
- HTTP 请求计划、retry 和 timeout 只控制尚未执行的 send。
- Error 的整流与逐项传播规则明确。
- Parallel 的顺序、背压、错误、取消和隔离规则通过测试。
- 文件、网络和进程失败不会泄漏资源或破坏旧输出。
- fmt 输出稳定，check 能发现基础语义错误。
- macOS/Linux 存在签名或带校验和的安装包。
- 文档示例由 CI 执行。
- 单元、集成、端到端、内存、模糊和跨平台测试通过。
- 已知限制公开记录。

公开限制集中维护在 [`KNOWN_LIMITATIONS.md`](KNOWN_LIMITATIONS.md)，发行包必须包含该文件；零散实现注释不能替代用户可见的限制清单。

功能数量不是 v1.0 的成功标准。成功标准是：用户安装 HHY 后，可以可靠地用同一种 Flow 思维处理文件、进程、网络与结构化数据；程序始终清晰地表达：

```text
source |> transform |> filter |> action
```

## 37. v1.0 实现符合性台账

本表记录当前实现证据，不改变前述规范，也不能用单个平台通过代替跨平台发布条件。

| 发布门槛 | 当前证据 | 状态 |
|---|---|---|
| Grammar、优先级、换行、Pipe 与 Map/Block | Lexer token 与 Parser AST 逐字快照；多错误恢复、严格 import grammar；`HHY_V1.md` 的 21 个完整示例和 README 示例均通过 Parser + Checker；stray `}` fuzz 超时回归 | macOS、Linux arm64、原生 Linux x86_64 已验证 |
| Value、String、Map、逻辑系统类型、GC 与资源 unwind | 嵌入 U+0000、UTF-8 入口、Bytes 与 BytesBuffer 区分、Result/File/Directory/FileEvent/Process/CommandResult tag、跨 worker 序列化、List/Map 深度相等、系统对象不泄漏 Map-only API、null 键存在性、GC 压力、max_memory、原子输出及文件/进程/HTTP 连续失败 unwind 回归 | macOS、Linux arm64、原生 Linux x86_64 ASan/UBSan 已验证 |
| 惰性 Flow、有限/大文件/无限事件流 | lazy、惰性 processes 快照、提前 close、屏障 PlanError、不可 Hash 的 group key 与不可比较的 sort key 拒绝、200000 项 GC Flow、五类验收程序 | macOS、Linux arm64、原生 Linux x86_64 已验证 |
| 文件、进程、HTTP、JSON、CSV、Regex | 本地文件、真实进程、本地 HTTP server、二进制 response、UTF-16 surrogate、CSV 多行与 PCRE2 限制测试 | macOS、Linux arm64、原生 Linux x86_64 已验证 |
| watch、parallel、取消与错误传播 | 原生 watcher、rename 归一化、FileEvent worker 快照、保序 worker、early close、fail-fast、Ctrl+C/timeout 回归 | macOS、Linux arm64、原生 Linux x86_64 已验证 |
| CLI、REPL、fmt、check、模块 | CLI exit code、REPL 多行 Pipe、fmt 幂等、Checker、模块缓存/导出/限定名缺失测试 | macOS、Linux arm64、原生 Linux x86_64 已验证 |
| Contract Registry、Execution Plan 与 EffectDispatcher | Checker/Runtime 共用 94 项 callable Registry；每项具有具体 input/output/threading 元数据；实现/登记一致性、占位元数据拒绝、qualified arity、唯一性校验、dry-run 文件/进程/网络 plan 与脱敏；dry-run parallel 不 fork 且保持惰性顺序值；Error stage 为 callable 名 | macOS、Linux arm64、原生 Linux x86_64 ASan/UBSan 已验证 |
| macOS arm64 Release 与 checksummed archive | `1.0.0` 原生 macOS arm64 完成 Debug ASan/UBSan、Release、完整测试、fuzz-smoke、文档与 checksummed archive；`BUILD_INFO.txt`、第三方 notices、SHA-256 均进入制品；可复核运行：[`#11 / 89ca409`](https://github.com/hh696-wq/hhy-vm/actions/runs/32817348334) | 已验证 |
| 覆盖引导 fuzz | Linux arm64 Clang/libFuzzer + ASan/UBSan 最新运行 16 秒、201496 次；此前发现并修复 Parser 恢复不前进超时，样本已进入 corpus；macOS fuzz-smoke 1000 输入通过；原生 Linux x86_64 CI 运行 libFuzzer + ASan/UBSan | Linux arm64 与原生 Linux x86_64 已验证 |
| Linux arm64 | `1.0.0` 在 GitHub Actions 原生 Ubuntu 24.04 arm64 完成架构断言、GCC 严格编译、Debug ASan/UBSan、Release、完整测试、Clang/libFuzzer、文档、archive 与 SHA-256；可复核运行：[`#11 / 89ca409`](https://github.com/hh696-wq/hhy-vm/actions/runs/32817348334) | 原生 CI 已验证 |
| Linux x86_64 | `1.0.0` 在 GitHub Actions 原生 Ubuntu 24.04 x86_64 完成架构断言、Debug ASan/UBSan、Release、94 项 contract 校验、完整测试、libFuzzer、文档执行、archive 内容与 SHA-256；可复核运行：[`#11 / 89ca409`](https://github.com/hh696-wq/hhy-vm/actions/runs/32817348334) | 原生 CI 已验证 |

只有所有发布条件均有可复核的通过证据，才允许把 `VERSION` 冻结为 `1.0.0`。异构 QEMU 不能替代原生 sanitizer 证据，也不能把模拟器启动失败记为实现通过或失败。

## Appendix A：核心 EBNF 草案

该 EBNF 是当前 Parser 与 Formatter 的语法基线；实现中发现冲突时必须先更新此处并补充回归测试。

```ebnf
program         = { separator | declaration } EOF ;
declaration     = let_decl | fn_decl | import_decl | export_decl | statement ;

let_decl        = "let" [ "mut" ] IDENT "=" expression terminator ;
fn_decl         = "fn" IDENT "(" [ parameters ] ")" block ;
parameters      = IDENT { "," IDENT } ;
import_decl     = "import" import_spec terminator ;
export_decl     = "export" ( let_decl | fn_decl ) ;
import_spec     = STRING [ "as" IDENT ]
                | "{" import_item { "," import_item } "}" "from" STRING
                | module_name ;
import_item     = IDENT [ "as" IDENT ] ;
module_name     = IDENT { "." IDENT } ;

statement       = if_stmt | for_stmt | while_stmt | try_stmt
                | return_stmt | break_stmt | continue_stmt
                | expression terminator ;
if_stmt         = "if" expression block
                  { "else" "if" expression block }
                  [ "else" block ] ;
for_stmt        = "for" IDENT "in" expression block ;
while_stmt      = "while" expression block ;
try_stmt        = "try" block "catch" IDENT block ;
return_stmt     = "return" [ expression ] terminator ;
break_stmt      = "break" terminator ;
continue_stmt   = "continue" terminator ;

block           = "{" { separator | declaration } "}" ;
expression      = assignment ;
assignment      = pipe [ "=" assignment ] ;
pipe            = coalesce { "|>" pipe_stage } ;
pipe_stage      = callable [ call_args ] [ closure ] ;
coalesce        = logical_or { "??" logical_or } ;
logical_or      = logical_and { "or" logical_and } ;
logical_and     = equality { "and" equality } ;
equality        = comparison { ( "==" | "!=" ) comparison } ;
comparison      = term { ( "<" | "<=" | ">" | ">=" ) term } ;
term            = factor { ( "+" | "-" ) factor } ;
factor          = unary { ( "*" | "/" | "%" ) unary } ;
unary           = ( "not" | "+" | "-" ) unary | postfix ;
postfix         = primary { call_args | index | member } ;
callable        = IDENT { member } ;
call_args       = "(" [ arguments ] ")" ;
arguments       = expression { "," expression } ;
index           = "[" expression "]" ;
member          = "." IDENT ;

closure         = "{" [ IDENT "->" ] closure_body "}" ;
closure_body    = expression | { separator | declaration } ;
primary         = literal | IDENT | list | map | attempt_expr
                | "(" expression ")" ;
attempt_expr    = "attempt" block ;
literal         = NULL | BOOL | INT | FLOAT | STRING | REGEX
                | BYTES | DURATION | PERCENT ;
list            = "[" [ expression { "," expression } [ "," ] ] "]" ;
map             = "{" [ map_entry { "," map_entry } [ "," ] ] "}" ;
map_entry       = ( IDENT | STRING ) ":" expression ;

terminator      = NEWLINE | ";" | implicit_before_rbrace ;
separator       = NEWLINE | ";" ;
```

闭包多语句体与 Map 字面量已由父语法上下文确定并通过 Parser 回归测试；Parser 不允许通过猜测字段名、换行或 token 内容改变 AST。后续修改这一消歧规则属于 grammar breaking change。
