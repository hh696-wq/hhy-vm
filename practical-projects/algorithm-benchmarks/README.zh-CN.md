# HHY 算法耗时基准：Go、Python、PHP 对照

这个项目用 HHY、Go、Python 和 PHP 分别实现相同的三种纯 CPU 算法，校验输出一致后比较独立进程的墙钟耗时。

## 算法

- `fibonacci(22)`：朴素双分支递归，结果 `17711`；
- `prime_count(3000)`：试除法统计质数，结果 `430`；
- `gcd_grid(70)`：计算 70×70 整数对的最大公约数校验和，结果 `14197`。

## 方法

- Apple Silicon arm64、macOS 26.6.2；
- HHY 1.1.0、Go 1.27.0、Python 3.14.7、PHP 8.5.9；
- 每个语言和算法预热 1 次，正式运行 5 次；
- 使用 `time.perf_counter_ns` 包围每个子进程，报告墙钟时间中位数；
- 包含语言进程启动时间；Go 在计时前构建，不包含编译时间；
- 基准脚本强制验证四种语言的算法名和校验值完全一致。

## 本机实测中位数

| 算法 | HHY | Go | Python | PHP |
|---|---:|---:|---:|---:|
| Fibonacci(22) | 1850.439 ms | 2.185 ms | 15.447 ms | 32.601 ms |
| Prime count(3000) | 666.554 ms | 2.113 ms | 15.210 ms | 31.969 ms |
| GCD grid(70) | 890.378 ms | 2.276 ms | 14.793 ms | 32.598 ms |

结果只描述这台机器、这些实现和这些输入。短任务中进程启动占 Go、Python 和 PHP 的较大比例；HHY 的时间主要来自解释执行密集循环和函数调用。HHY 当前更适合文件、进程、HTTP 和结构化数据编排，而不是替代编译型语言执行数值热点。

## 运行

在仓库根目录执行：

```sh
sh practical-projects/algorithm-benchmarks/run.sh
```

输出：

- `results/benchmark.csv`：每项中位数、最小值、最大值、校验值和运行次数；
- `results/report.json`：测试方法、运行时版本、机器环境和完整结果。
