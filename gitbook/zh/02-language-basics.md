# 2. 语言基础

变量、值、函数、条件、循环和作用域。

## 2.1 动态类型是什么意思

HHY 的变量声明不写类型，值在运行时携带自己的逻辑类型。动态类型不等于随意转换：条件必须得到 Bool，String 不会自动变成 Number、Bool 或 Path，参数数量和不支持的运算都会产生结构化错误。用 type(value) 查看类型，用 is_type(value, name) 判断类型。


```hhy
let nothing = null
let enabled = true
let count = 42
let ratio = 0.75
let title = "HHY"
let pattern = /ERROR|WARN/i
let names = ["Ada", "Linus"]
let user = { name: "Ada", active: true }
let indexes = 0..3
let size_limit = 10mib
let timeout_limit = 5s
let completion = 80%

print(type(user))
print(is_type(title, "String"))
```


## 2.2 标量与单位类型

| 类型 | 示例 | 用途 |
| --- | --- | --- |
| Null | null | 表示没有值 |
| Bool | true | 条件与谓词 |
| Int | 42 | 整数计算 |
| Float | 3.14 | 浮点计算 |
| String | "hello" | UTF-8 文本 |
| Regex | /ERROR/i | 文本匹配 |
| Bytes | 10mib | 文件或内存大小 |
| Duration | 5s | 超时与时间间隔 |
| Percent | 80% | 比例 |
| DateTime | now() | 带时区时间 |
| Path | path("logs") | 文件系统路径 |


String、数字、单位和 Path 的精确边界行为属于 Reference。日常脚本只需记住：HHY 不会在 String、Number、Bool 和 Path 之间做隐式转换。


[查看类型与语法参考 →](/zh/learn/syntax-reference)

查阅 UTF-8、数值溢出、运算符和字面量的精确定义。


## 2.3 List、Map 与 Range

List 使用从 0 开始的索引，越界产生 IndexError。Map 的键只能是 String，保持插入顺序；map.key 与 map["key"] 等价。普通缺失键返回 null，require 用于区分“键缺失”和“键存在但值是 null”。Range a..b 包含 a、不包含 b，并且不会预先分配 List。


```hhy
let original = ["Flow", "System"]
let extended = append(original, "Pipe")
let shortened = remove_at(extended, 1)

let config = { retries: 3, label: null }
let updated = put(config, "timeout", 5s)
let selected = pick(updated, ["retries", "timeout"])

print(original)
print(shortened)
print(get(config, "missing"))
print(require(config, "label"))
print(selected)
```


List 和 Map 不原地修改。append、remove_at、put、remove_key、pick 都返回新集合，所以示例中的 original 和 config 保持不变。List/Map 支持深度相等；Function、Stream 和系统资源对象不支持值相等。


## 2.4 Result、Stream 与系统对象

| 类型 | 用于 |
| --- | --- |
| Result | 显式保存一次操作的成功值或 Error |
| Stream | 惰性处理文件、行、进程、响应和事件 |
| Error | 携带类别、位置和 Flow stage 的失败 |
| Function | 用户函数与闭包 |
| 系统对象 | File、Process、HttpResponse 等带只读字段的专用值 |


系统对象不是 Map。需要写入 JSON 时，先用 map 或 pick 选择普通字段。Stream 的惰性和消费规则在 Flow 章节展开。


## 2.5 变量、作用域与不可变性

```hhy
let service = "api"
let mut retries = 0
retries = retries + 1
```


let 创建不可重新赋值的绑定；需要重新赋值时使用 let mut。List 和 Map 的更新函数返回新值，不修改原集合。变量遵循块级词法作用域，并且必须先声明后使用。


{% hint style="info" %}
闭包可以捕获外层值。捕获 let mut 的闭包不能发送到 parallel worker；并发限制在“并发与监听”章节说明。
{% endhint %}


## 2.6 条件、循环与函数

```hhy
fn classify(score) {
    if score >= 90 { return "excellent" }
    else if score >= 60 { return "pass" }
    else { return "retry" }
}

let mut total = 0
for score in [98, 72, 55] {
    if score < 60 { continue }
    total = total + score
}

let mut attempts = 0
while attempts < 3 {
    attempts = attempts + 1
}

print(classify(98))
print(total)
```


支持 if / else if / else、for item in iterable、while、break 和 continue。for 可以遍历 List、Map entries、Range 或 Stream；遍历 Stream 会消费它。函数使用位置参数，参数数量在调用时检查；没有显式 return 时返回 null。


```hhy
fn summarize(items) {
    let mut total = 0

    for item in items {
        if item.enabled {
            total = total + item.score
        }
    }

    return total
}

let users = [
    { name: "Ada", enabled: true, score: 98 },
    { name: "Linus", enabled: false, score: 86 }
]

summarize(users) |> print
```


闭包写作 { item -> expression }；多条语句时必须显式写参数并用 return 返回。单参数闭包在明确的 Flow 上下文中可以使用 { it * 2 }。V1.2.0 不支持重载、泛型或默认参数。
