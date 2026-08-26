# FlowGuard

FlowGuard 是一个完全使用 HHY v1.0 编写的代码仓库体检与质量门禁应用。它不是语法演示，而是一个包含模块、配置、测试数据、HTTP 测试服务、报告验证和端到端自测的完整项目。

## 展示的 HHY 能力

- 使用模块拆分结构检查、文件检查、命令检查、HTTP 检查和报告生成；
- 使用 Path、`files` 和 Stream 遍历真实项目；
- 使用 Regex 扫描疑似凭据，但不把匹配内容写入报告；
- 使用 `parallel` 并发执行质量命令和 HTTP 健康检查；
- 使用 `attempt` 隔离单项失败并继续完成整份报告；
- 使用原生 Bytes 比较文件大小；
- 原子写入结构化 JSON 报告；
- 无失败返回退出码 0，有失败返回退出码 1，参数错误返回退出码 3。

## 项目结构

```text
flowguard/
├── flowguard.hhy           # 应用入口
├── lib/                    # 六个 HHY 业务模块
├── config/                 # 健康与风险场景配置
├── fixtures/               # 可以确定性复现的项目数据
├── test-server.py          # 本地 HTTP 健康接口
├── test-report.py          # JSON 报告断言
└── self-test.sh            # 完整端到端自测
```

## 一键自测

在 HHY 仓库根目录执行：

```sh
sh practical-projects/flowguard/self-test.sh
```

测试会启动仅监听 `127.0.0.1:18991` 的临时服务，然后执行两个场景：

1. `healthy-project` 的必需文件、文件扫描、两个并发命令和健康接口必须全部通过，FlowGuard 返回 0。
2. `risky-project` 故意缺少 LICENSE，包含假的 `DEMO_TOKEN`，运行一个失败命令并访问 404 端点，FlowGuard 必须生成报告并返回 1。

报告生成在 `practical-projects/flowguard/output/`，该目录已加入 `.gitignore`。

## 手动运行

先启动测试接口：

```sh
cd practical-projects/flowguard
python3 test-server.py
```

另开终端运行健康项目：

```sh
cd practical-projects/flowguard
../../build/hhy run flowguard.hhy \
  fixtures/healthy-project \
  config/healthy.json \
  output/healthy-report.json
```

## 配置格式

```json
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

当前大小限制支持 `256b`、`1kib`、`4kib` 和 `1mib`。命令始终以 argv 数组交给 `run`，不会通过 `shell` 拼接；单个命令限制为 15 秒和 1 MiB 输出。

## 检查真实项目

复制一份配置并修改后运行：

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
