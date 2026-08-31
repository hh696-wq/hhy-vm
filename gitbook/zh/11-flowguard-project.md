# 11. 实战项目：FlowGuard

用 HHY v1.2.0 构建完整的代码仓库体检与质量门禁应用，包含真实数据、并发检查、JSON 报告和端到端测试。

## 11.1 这不是语法 Demo

FlowGuard 是一个使用 HHY v1.2.0 运行并通过自测的完整应用。它接收项目目录与 JSON 配置，检查必需文件、扫描文件和疑似凭据、并发执行质量命令与 HTTP 健康检查，最终原子写入结构化报告，并用稳定退出码决定质量门禁是否通过。


| 应用能力 | 使用的 HHY 能力 |
| --- | --- |
| 项目结构检查 | Path、read_text、attempt、List |
| 文件与安全扫描 | files、Stream、Regex、Bytes |
| 质量命令 | run、parallel、Duration、CommandResult |
| 服务健康检查 | http.get、timeout、retry、parallel |
| 报告与门禁 | Map、encode_json、原子 save_text、exit |


[在 GitHub 查看 FlowGuard 完整源码 ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/flowguard)

包含 HHY 入口、六个业务模块、配置、测试项目、HTTP 服务和报告断言。


## 11.2 项目目录

入口脚本只负责编排，具体检查被拆分到 lib 中；config 保存两套场景，fixtures 提供可以重复测试的项目数据。output 和 __pycache__ 被忽略，不进入仓库。


![FlowGuard 项目目录树，展示 config、fixtures、lib、入口脚本和测试工具](https://hhylang.dev/flowguard-project-tree-v2.png)

_FlowGuard 的真实目录结构；output 与 __pycache__ 是本地自测产物，不在 Git 中。_


| 路径 | 职责 |
| --- | --- |
| flowguard.hhy | 读取参数和配置，组合检查，写报告并设置退出码 |
| lib/*.hhy | 结构、文件、安全、命令、健康和报告模块 |
| config/*.json | 健康与风险场景配置 |
| fixtures/* | 确定性的被检查项目数据 |
| self-test.sh | 启动测试服务并验证两个端到端场景 |


## 11.3 运行完整自测

在仓库根目录执行一条命令。自测会启动仅监听 127.0.0.1:18991 的临时 HTTP 服务，先检查 HHY 模块，再运行健康和风险两套场景，并用 Python 对生成的 JSON 报告做结构与结果断言。


```sh
cd hhy-vm
sh practical-projects/flowguard/self-test.sh
```


![FlowGuard 端到端自测的终端输出，健康场景全部通过，风险场景发现五项失败，最终自测通过](https://hhylang.dev/flowguard-self-test-v2.png)

_真实运行结果：healthy-service 8 项通过；risky-service 正确发现 5 项失败；最终 FlowGuard self-test passed。_


## 11.4 健康场景与风险场景

| 场景 | 输入数据 | 预期结果 |
| --- | --- | --- |
| healthy-service | README、LICENSE、package.json、源码、两个成功命令和 2xx 健康端点 | 8 passed，退出码 0 |
| risky-service | 缺少 LICENSE、模拟 DEMO_TOKEN、失败命令和 404 端点 | 5 failed，退出码 1；自测将这个非零状态视为正确结果 |


{% hint style="info" %}
风险场景里的凭据是明确标注的假数据。FlowGuard 报告只保存文件名和 content_redacted: true，不保存匹配内容。
{% endhint %}


## 11.5 配置自己的项目

```text
{
  "project": { "name": "my-service" },
  "required_files": ["README.md", "LICENSE"],
  "limits": { "large_file": "4kib" },
  "commands": [
    { "name": "tests", "argv": ["npm", "test"] }
  ],
  "health_checks": [
    { "name": "api", "url": "http://127.0.0.1:8080/health" }
  ]
}
```


命令使用 argv 数组直接交给 run，不经过 shell 拼接；每个命令限制为 15 秒和 1 MiB 输出。当前示例接受 256b、1kib、4kib 或 1mib 文件阈值。


```sh
hhy run \
  --limit max_runtime=2min \
  --limit max_memory=256mib \
  --limit max_processes=8 \
  practical-projects/flowguard/flowguard.hhy \
  /path/to/project \
  practical-projects/flowguard/config/my-project.json \
  report.json
```


## 11.6 为什么它能代表 HHY

FlowGuard 把文件系统、进程、HTTP 和数据处理统一进同一条可靠工作流。单项失败通过 attempt 转换为结构化检查结果，不会阻止其他检查完成；parallel 提供有上限的并发；最终报告可以直接交给 CI/CD 读取。这正是 HHY 相比复杂 Shell 脚本最有辨识度的应用方向。


[阅读 FlowGuard 中文使用说明 ↗](https://github.com/hh696-wq/hhy-vm/blob/main/practical-projects/flowguard/README.zh-CN.md)

查看配置字段、手动运行方式、测试设计与真实项目接入命令。
