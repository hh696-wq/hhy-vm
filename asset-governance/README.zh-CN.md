# Asset Governance

Asset Governance 是使用 HHY v1.0 编写的完整文件资产治理项目。审计器扫描源码、配置、图片、视频和构建产物，检查超大文件、旧文件、命名规则、重复文本内容和疑似凭据，生成包含整改动作的 JSON 报告；清理器只执行报告中的 `copy`、`move` 和 `remove`。

## 一键自测

```sh
cd /path/to/hhy-vm
sh asset-governance/self-test.sh
```

自测使用 `mktemp` 创建隔离工作区，首先验证审计报告，然后运行：

```sh
hhy run --dry-run asset-governance/cleanup.hhy PROJECT report.json
hhy run asset-governance/cleanup.hhy PROJECT report.json
```

测试会断言 dry-run 后没有文件发生变化，正式执行后 copy、move、remove 三个动作全部生效。敏感内容始终脱敏，报告只记录文件名。
