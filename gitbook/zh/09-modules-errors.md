# 9. 模块与错误

组织代码，传播结构化错误并可靠清理资源。

## 9.1 导入形式与路径解析

```hhy
import { add } from "./math.hhy"

add(20, 22) |> print
```


| 写法 | 用途 |
| --- | --- |
| import "./lib/report.hhy" as report | 导入本地模块命名空间 |
| import { parse } from "./lib/data.hhy" | 具名导入 |
| import { validate as check } from "./lib/data.hhy" | 具名导入并设置别名 |
| import http | 导入标准库模块 |


相对路径基于当前源码文件目录。标准库使用裸名称；本地文件显式使用 ./、../ 或绝对 Path。


## 9.2 export、作用域与执行

```hhy
export let version = "1.0"

export fn normalize_name(name) {
    return name |> trim |> lower
}

fn internal_helper() {
    return null
}
```


只有 export 名称对外可见。模块拥有独立顶层作用域，并在首次 import 时执行一次、随后缓存。循环依赖在执行前产生 CheckError。V1.2.0 支持标准库、本地模块，以及通过本地包安装的进程扩展模块。


## 9.3 Error 的字段与类别

所有失败都使用 Error，而不是靠 null 或打印文本表达。Error 提供 kind、code、message、source、stage、cause、stack、context 字段；敏感 header、凭据和完整文件内容不会默认进入 context。


内置类别包括 SyntaxError、CheckError、TypeError、ValueError、IndexError、KeyError、EncodingError、IoError、ProcessError、HttpError、HttpStatusError、TimeoutError、CancelledError、ResourceLimitError 和 PlanError。


## 9.4 try/catch 与重新抛出

```hhy
try {
    read_text(path("config.json"))
        |> parse_json
        |> print
} catch err {
    print_error(err)
    exit(1)
}
```


catch 捕获 try 块传播出的第一个错误。catch 正常结束后脚本继续；无法处理时用 throw(err) 保留错误链重新抛出。未处理错误让脚本以非零状态退出。


## 9.5 Flow 错误与单项 Result

```hhy
path("./configs")
    |> files("**/*.json")
    |> map { file -> attempt { read_text(file.path) } }
    |> where { result -> result.ok }
    |> map { result -> result.value }
    |> print
```


Stream 中未处理的 Error 默认终止整条 Pipeline。attempt 把单次操作转换为 Result，适合批处理中显式保留成功项和失败项。on_error 用于替换整条失败的上游 Stream，不会自动跳过错误项。


## 9.6 资源清理保证

Error、return、exit、timeout、Ctrl+C 和 cancel 都走统一 unwind。Stream close 幂等；原子保存失败会删除临时文件并保留旧文件；子进程、HTTP response、watcher 和 worker 都会响应取消。
