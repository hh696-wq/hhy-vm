# 19. CLI 参考

运行、检查、格式化、REPL、dry-run 与性能分析。

## 19.1 版本与发布信息

使用 --version 确认当前二进制版本、项目作者、开源许可证和官方联系方式。源码构建使用 ./build/hhy；发行包或安装到 PATH 后可直接使用 hhy。


### HHY · 版本信息

```console
$ ./build/hhy --version
hhy 1.2.0
© 2026 HHY Language contributors
Author: houhuiyang
License: Apache License 2.0
https://hhylang.dev/
huiyang.hou@qq.com
```

HHY 1.2.0 的真实命令输出；版本、作者、许可证、官网和联系邮箱由 CLI 直接提供。


{% hint style="info" %}
如果从正式发行包运行，请在解压目录执行 ./bin/hhy --version；如果已经 make install 或加入 PATH，则执行 hhy --version。
{% endhint %}


## 19.2 完整命令

```sh
hhy script.hhy [args...]
hhy run script.hhy [args...]
hhy repl
hhy check script.hhy...
hhy fmt script.hhy...
hhy fmt --check script.hhy...
hhy ast script.hhy
hhy tokens script.hhy
hhy run --dry-run script.hhy
hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
hhy profile script.hhy [args...]
hhy profile --cpu script.hhy
hhy profile --heap --format json --output profile.json script.hhy
hhy --version
hhy --help
```


| 命令 | 用途 |
| --- | --- |
| hhy run | 运行脚本并传递 args |
| hhy profile | 分析 CPU 热点、调用次数和托管 Heap 分配 |
| hhy repl | 启动交互环境 |
| hhy check | 检查语法和核心语义 |
| hhy fmt | 写入官方格式 |
| hhy fmt --check | 只检查格式 |
| hhy ast | 输出 AST |
| hhy tokens | 输出 Lexer Token |
| hhy run --dry-run | 预览脱敏执行计划 |


hhy script.hhy 是 hhy run script.hhy 的简写。脚本参数可能以 - 开头时，在 Runtime 选项后使用 -- 分隔。


## 19.3 CPU 与 Heap 性能分析

profile 会真实执行脚本，默认在一次运行中同时收集 CPU 和托管 Heap 数据。报告写入 stderr，因此脚本 stdout 保持不变；命令返回脚本原有退出码。


```sh
hhy profile examples/09-profile-algorithms.hhy -- fibonacci 20
hhy profile --cpu examples/09-profile-algorithms.hhy fibonacci 20
hhy profile --heap --format json --output profile.json examples/09-profile-algorithms.hhy fibonacci 20
```


| 选项 | 行为 |
| --- | --- |
| --cpu | 只收集 1ms 进程 CPU 采样和调用次数 |
| --heap | 只收集累计分配、分配次数、Heap 峰值和 GC 后占用 |
| --format text\|json | 选择人类可读或机器可读报告；默认 text |
| --output <path> | 把报告写入文件，而不是 stderr |
| --limit NAME=VALUE | 与 run 相同，覆盖 Runtime 资源限制 |
| --dry-run | 与 run 相同，拦截外部副作用并分析计划执行 |


```console
$ hhy profile examples/09-profile-algorithms.hhy -- fibonacci 20
HHY profile: examples/09-profile-algorithms.hhy

Summary
  Wall time        0.006 s
  CPU time         0.004 s
  CPU utilization  64.5%
  CPU samples      2
  Heap peak        755.9 KiB
  Heap after GC    4.0 KiB
  Allocated        523.9 KiB
  Allocations      11107

CPU hotspots
  CPU%    Samples      Calls  Function
  100.0%        2      21891  fibonacci  examples/09-profile-algorithms.hhy:5:1

Allocation hotspots
  Bytes          Objects  Function
  515.6 KiB        10966  fibonacci  examples/09-profile-algorithms.hhy:5:1

fibonacci 6765
```


{% hint style="info" %}
CPU 使用进程 CPU 时间采样，文件、HTTP 和进程等待不会被误算成 CPU 热点。运行不足数毫秒的脚本可能样本太少，应增大输入或重复负载。Heap 只统计 HHY 的 Boehm GC 托管内存，不包含扩展子进程或原生库自行管理的内存。
{% endhint %}


## 19.4 解释器性能架构演进

v1.2.0 继续保留 AST Interpreter，通过预解析、静态槽位和可复用轻量调用帧降低函数调用成本。确定的局部变量走 Slot 快路径；闭包捕获、全局变量与 builtin 保留兼容的 Env 路径。


{% hint style="info" %}
本节的交互式图表请在 [hhylang.dev](https://hhylang.dev/zh/learn/cli-reference) 查看。
{% endhint %}


{% hint style="info" %}
Bytecode VM 是数据驱动的后续方向，并非当前执行前提。只有当 Profile 证明 AST dispatch 已成为主要剩余热点时，才进入该阶段。
{% endhint %}


## 19.5 Runtime 资源限制

run 的 --limit NAME=VALUE 可以重复出现。大小必须带 b/kb/mb/gb/kib/mib/gib，时间必须带 ns/us/ms/s/min/h，计数值不带单位。


```sh
hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
```


| 限制 | 默认值 |
| --- | --- |
| max_memory | 512mib |
| max_open_files | 256 |
| max_processes | 16 |
| max_parallelism | 16 |
| max_http_body | 16mib |
| max_regex_steps | 1000000 |
| max_recursion | 256 |
| max_runtime | 0（CLI 默认不设总时限） |


## 19.6 稳定退出码

```text
0  成功
1  未处理的运行时错误
2  语法或静态检查错误
3  CLI 用法错误
4  文件 I/O、进程或网络错误
5  超时或取消
```


自动化脚本应按稳定退出码而不是错误文本进行分支。
