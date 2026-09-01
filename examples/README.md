# HHY v1.1 典型脚本

> 当前版本：`1.3.10`；示例保持兼容 v1.0 冻结语义并持续运行 AST/Bytecode 双引擎及真实负载回归测试。

这些示例按照 [`docs/HHY_V1.md`](../docs/HHY_V1.md) 中的当前语法编写，用于验证 HHY 的核心定位：通过统一的管道模型连接文件、进程、网络与结构化数据。

| 脚本 | 应用场景 | 核心能力 |
|---|---|---|
| `01-log-errors.hhy` | 汇总大型日志中的错误 | files、Units、parallel、read_lines |
| `02-active-users.hhy` | 请求 API 并导出活跃用户 | HTTP Request、retry、JSON |
| `03-process-monitor.hhy` | 找出高资源占用进程 | processes、where、sort_by |
| `04-watch-and-build.hhy` | 监听源码并运行构建 | watch、debounce、run |
| `05-csv-report.hhy` | 从 CSV 生成部门统计 | CSV、group_by、map |
| `06-backup-large-files.hhy` | 归档最近修改的大文件 | files、DateTime、copy、dry-run |
| `07-language-basics.hhy` | 展示变量、函数、循环和错误 | let、fn、for、if、try/catch |
| `08-batch-health-check.hhy` | 并发检查多个服务 | parallel、HTTP、attempt、Result |
| `09-profile-algorithms.hhy` | 定位递归算法的 CPU 与分配热点 | profile、递归、函数调用 |

预期执行方式：

```bash
hhy examples/01-log-errors.hhy ./logs ./output/errors.txt
hhy check examples/01-log-errors.hhy
hhy fmt --check examples/01-log-errors.hhy
hhy profile examples/09-profile-algorithms.hhy -- fibonacci 20
```

当前解释器已经可以解析、检查、格式化并执行这些脚本。需要网络、监听目录或特定输入文件的示例由测试套件使用本地夹具和本地 HTTP 服务验证，避免 CI 依赖公网与机器状态。规范调整时必须同步更新示例和对应验收测试。
