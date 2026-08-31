# 13. 实战项目：Asset Governance

扫描项目资产、生成治理报告，并用 Runtime 原生 dry-run 安全执行 copy、move、remove 整改动作。

## 13.1 审计与整改分离

Asset Governance 由 audit.hhy 和 cleanup.hhy 两个程序组成。审计器扫描源码、配置、图片、视频和构建产物，发现超大、过旧、命名不规范、重复文本内容和疑似凭据；清理器只接受审计报告中的白名单动作，不通过 shell 拼接命令。


| 检查或动作 | HHY 实现 |
| --- | --- |
| 文件清单和大小 | files、File.size、Bytes |
| 旧文件 | File.modified、now、Duration |
| 命名与敏感信息 | Regex、read_text、脱敏 finding |
| 重复内容 | group_by 文本内容，不把原文写进报告 |
| 整改 | copy、move、remove 与 --dry-run EffectDispatcher |


[在 GitHub 查看 Asset Governance 完整源码 ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/asset-governance)

包含审计器、清理器、四个 HHY 模块、风险 fixtures 和 dry-run/正式整改断言。


## 13.2 项目目录

![Asset Governance 项目目录树](https://hhylang.dev/asset-governance-tree.png)

_真实目录包含审计与清理入口、治理模块，以及故意准备的大文件、旧文件、重复文件和敏感配置 fixtures。_


| 程序 | 职责 |
| --- | --- |
| audit.hhy | 扫描项目并原子生成 report.json；存在 critical finding 时返回 1 |
| cleanup.hhy | 读取 report.actions，执行受控 copy/move/remove |
| self-test.sh | 创建隔离 mktemp 工作区，先 dry-run 再正式整改并逐项断言 |


## 13.3 实际自测与 dry-run

```sh
cd hhy-vm
sh practical-projects/asset-governance/self-test.sh
```


![Asset Governance 审计、dry-run 和正式整改的真实终端输出](https://hhylang.dev/asset-governance-self-test.png)

_真实运行：识别 large/naming/stale/sensitive/duplicate finding；dry-run 输出副作用计划且文件不变；正式执行三个动作后断言通过。_


{% hint style="info" %}
截图中的 Processed 表示程序走到了该动作；dry-run 阶段由 Runtime 拦截副作用。测试随后确认目标目录完全未改变，正式运行后才验证 copy、move 和 remove 生效。
{% endhint %}


## 13.4 两阶段运行

先运行审计并阅读 report.json。存在 critical finding 时 audit 返回 1，但报告仍完整生成。确认 actions 后先执行 dry-run，检查 Runtime 输出的 effect 计划，最后再正式整改。


```sh
hhy run practical-projects/asset-governance/audit.hhy ./project ./config.json ./report.json
hhy run --dry-run practical-projects/asset-governance/cleanup.hhy ./project ./report.json
hhy run practical-projects/asset-governance/cleanup.hhy ./project ./report.json
```
