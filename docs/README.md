# HHY 文档中心

更新日期：2026-09-09。当前 Core 为 **1.7.0**，语言规范冻结为 **1.0.0**，Database 扩展独立版本为 **1.0.0**。版本基线以 [VERSION](../VERSION) 为准。

## 从哪里开始

| 目的 | 阅读入口 |
| --- | --- |
| 首次安装并运行脚本 | [安装说明](../INSTALL.md) → [当前版本使用指南](CURRENT_VERSION.md) → [示例目录](../examples/README.md) |
| 了解当前版本、开关与回退 | [1.7.0 使用指南](CURRENT_VERSION.md) → [已知限制](KNOWN_LIMITATIONS.md) |
| 学习语言和标准库 | [中文规范](HHY_V1.md) / [English specification](HHY_V1.en.md) |
| 查语法和兼容语义 | [中文规范](HHY_V1.md) / [English specification](HHY_V1.en.md) |
| 开发 Web 或数据库应用 | [Web Runtime](../WEB_RUNTIME.md) → [Database API](../extensions/database/README.md) → [数据库验收](../extensions/database/ACCEPTANCE.md) |
| 开发、安装和分发扩展 | [扩展入口](../extensions/README.md) → [进程协议](EXTENSION_PROTOCOL_V1.md) → [签名 Registry](EXTENSION_REGISTRY_V1.md) |
| 修改 VM 或 Compiler | [Bytecode](BYTECODE.md) → [Runtime 治理](RUNTIME_GOVERNANCE.md) → [Compiler IR](../COMPILER_IR.md) |
| 评估项目现状与后续工作 | [健康报告](LANGUAGE_HEALTH_REPORT.md) → [主迭代路线图](ROADMAP.md) |

## 架构、依赖与工具

- VM 专项：[调用与展开](../VM_CALL_RUNTIME.md)、[Inline Cache](../VM_INLINE_CACHE.md)、[GC 与调度](../VM_GC_SCHEDULER.md)。这些文档中的 v1.6.x 是工程阶段编号，相关实现随产品 1.7.0 交付，并不表示存在独立的 v1.6.x 发行版。
- 依赖：[构建依赖](DEPENDENCIES.md)、[第三方 Notices](THIRD_PARTY_NOTICES.md)。发行包的实际依赖版本以包内 `BUILD_INFO.txt` 为准。
- 工具：[CLI 与执行指南](CURRENT_VERSION.md)、[语言 CLI 规范](HHY_V1.md#27-cli)、[真实工作负载回归](../tests/workloads/README.md)。编辑器插件与完整案例不随 Core 源码分发。
- 演示：[终端演示](TERMINAL_DEMO.md)。推广草稿、设计素材和视频留在主仓之外。

## 版本记录与规划

当前发行说明见 [1.7.0](../RELEASE_NOTES_1.7.0.md)，前序关键版本见 [1.5.0](../RELEASE_NOTES_1.5.0.md) 和 [1.4.3](../RELEASE_NOTES_1.4.3.md)。[历史发行索引](releases/README.md) 汇总旧版本，保留原始日期、结论及证据。

[扩展路线图](EXTENSION_ROADMAP.md) 顶部是当前状态，后面的 Office 排期等属于早期设计归档；当前迭代顺序和完成标准统一维护在[主路线图](ROADMAP.md)。当前下一重点是 v1.8 外部采用与旗舰场景，优化默认准入继续单独按数据评估。

## 文档维护约定

1. 语言语义由中文冻结规范定义；英文保持同步。新增兼容实现写在规范开头的增量说明，不把历史 v1.0 条目整体改成新版本。
2. 当前状态写清 Core、语言规范和扩展三个独立版本。工程阶段完成、正式发行、正确性验收和性能默认准入分别记录。
3. 性能结论必须带日期、版本/commit、平台、输入、样本方式和原始证据；历史数字不自动升级为当前版本结果。
4. 新文档在本页登记；内部链接相对当前文件定位。正式能力的 API 细节链接到维护源，避免复制多份后漂移。
5. 更新代码示例后使用现有 `tests/check-docs.sh` 检查完整 HHY 代码块；规范中的语法片段和反例不当作独立可执行脚本。
6. 本目录的公开文档纳入 Git；推广草稿除外。原始性能分析、官网、完整案例和编辑器源码留在本地或独立维护。修改相对链接后运行 `python3 scripts/check-repository.py`，该检查只认可 Git 跟踪的目标。
