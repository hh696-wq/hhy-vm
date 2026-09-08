# 6. 进程与系统

运行命令、消费输出并检查系统状态。

## 6.1 run 与 shell 的安全边界

```hhy
run(["git", "log", "--oneline"], { timeout: 5s })
    |> stdout_lines
    |> take(10)
    |> print
```


run(argv, options?) 直接把 List<String> 交给操作系统，不经过 Shell，因此空格、通配符、$、重定向和管道不会被二次解释。只有确实需要 Shell 语法时才使用 shell(command, options?)；Checker 会对 shell 给出安全提示。


| 选项 | 用途 |
| --- | --- |
| cwd | 子进程工作目录 Path |
| env | 只覆盖子进程环境 |
| stdin | 传给命令的标准输入文本 |
| timeout | 命令最长运行时间 |
| max_output | stdout 与 stderr 捕获上限 |


{% hint style="info" %}
包含用户输入时优先使用 run。shell 会解释重定向、管道和变量展开，只在明确需要 Shell 语义时使用。
{% endhint %}


## 6.2 CommandResult

run 与 shell 默认等待结束并返回 CommandResult，而不是把非零退出码自动当作 HHY Error。脚本应读取 exit_code 决定业务成功。


| 字段 | 内容 |
| --- | --- |
| exit_code | 子进程退出码 |
| stdout | 标准输出 String |
| stderr | 标准错误 String |
| duration | 命令运行 Duration |


非零 exit_code 不会自动变成 HHY Error。脚本需要根据命令约定判断成功。stdout_lines(result) 可把已捕获输出作为行 Stream 继续处理。


## 6.3 进程快照与字段

processes() 返回当前时刻的 Stream<Process> 快照。Process 不是 Map，提供 pid、name、cpu、memory、status、command 只读字段；转 JSON 前必须显式映射成普通 Map。


{% hint style="info" %}
排序示例使用 sort_by({ order: "desc" })；order 只能是 asc 或 desc，默认 asc。排序是稳定 barrier，会物化有限快照。
{% endhint %}


## 6.4 args、env、system 与 stdin

| 值 | 内容 |
| --- | --- |
| args | 不含脚本路径的 List<String> |
| env | 只读环境变量视图 |
| system | OS、架构、主机、CPU、内存和目录信息 |
| stdin_lines() | 标准输入行 Stream |


```hhy
if length(args) != 1 {
    print_error("usage: script.hhy <input>")
    exit(3)
}

let input = path(args[0])
```


## 6.5 查阅完整 API

[进程与系统 API Reference →](/zh/learn/standard-library#fn-run)

查阅 run、shell、processes、stdin_lines 和 every 的完整签名。
