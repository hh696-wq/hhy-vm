# 17. 语法完整参考

V1.2.0 的词法、字面量、运算符、语句、闭包和模块语法。

## 17.1 源文件与词法

| 项目 | 规则 |
| --- | --- |
| 文件 | .hhy、UTF-8、LF 或 CRLF |
| 标识符 | 大小写敏感；ASCII 字母、数字和下划线，不能以数字开头 |
| 语句结束 | 换行或可选分号 |
| 续行 | 未闭合括号或行首/行尾的 \|> |
| 注释 | # 单行注释；首行允许 shebang |
| / | 表达式起点为 Regex，已有左操作数后为除法 |


## 17.2 字面量与原生单位

```hhy
let nothing = null
let flags = [true, false]
let numbers = [42, -10, 0xff, 0b1010, 1.5, 1e6]
let name = "HHY"
let strings = ["hello", "Hello, {name}"]
let pattern = /ERROR|WARN/i
let list = [1, 2, 3]
let record = { name: "Tom", age: 20 }
let interval = 1..10
let units = [10mib, 5s, 80%]
```


Range 包含起点、不包含终点。Bytes 支持 b/kb/mb/gb/kib/mib/gib；Duration 支持 ns/us/ms/s/min/h；紧贴数字的 % 是 Percent。String 支持插值以及 \, ", \n, \r, \t, \b, \f, \0 转义。


## 17.3 运算符优先级（高到低）

```text
()  []  .
not  -  +
*  /  %
+  -
<  <=  >  >=
==  !=
and
or
??
|>
=
```


and、or 与 ?? 短路执行；= 只允许给 let mut 绑定赋值；|> 左结合。条件必须是 Bool，不存在把 0、空字符串或 null 自动当作 false 的规则。


## 17.4 声明、控制流、函数与模块

```hhy
let name = "HHY"
let mut count = 0
count = count + 1
let enabled = true
let items = ["Flow", "Pipe"]

if enabled { print("yes") } else { print("no") }
for item in items { print(item) }
while count < 3 { count = count + 1 }

fn add(a, b) { return a + b }
let doubled = [1, 2] |> stream |> map { number -> number * 2 } |> collect

try { read_text(path("config.json")) } catch err { print_error(err) }
let result = attempt { read_text(path("config.json")) }

import { add as sum_two } from "./math.hhy"
export fn public_api(value) { return value }
```


| 结构 | 形式 |
| --- | --- |
| 调用 | name(args) |
| Pipe | x \|> f(a) 等价于 f(x, a) |
| 闭包 | { param -> expression } |
| Map | { key: value } |
| 控制流 | if、for、while、break、continue、return |
| 模块 | import、as、export |


## 17.5 核心值类型

```text
Null Bool Int Float String Regex BytesBuffer
List Map Range Function Error Result Stream
Bytes Duration Percent DateTime Path
File Directory FileEvent Process CommandResult
HttpRequest HttpResponse
```


{% hint style="info" %}
HHY 是动态类型语言，但不会进行危险的 String/Number 或 String/Bool 隐式转换。Int 是有符号 64 位整数，Float 是 IEEE 754 double。
{% endhint %}
