# HHY 1.7.0 使用与维护指南

更新日期：2026-09-09；依据仓库代码、版本化策略与 2026-09-08 发布验收归档整理。本文是当前实现说明，语言语义仍以 [v1.0 规范](HHY_V1.md) 为准。

## 版本与交付边界

| 项目 | 当前状态 |
| --- | --- |
| Core | 1.7.0，包含 v1.6 Runtime 实验及 v1.7 Compiler 工程阶段 |
| 语言规范 | 1.0.0 冻结；既有 Pipe、Value、Stream、Error 和取消语义保持兼容 |
| 默认执行 | 直接编译到经验证的 Bytecode；AST 保留为语义对照和显式回退 |
| 可选 Compiler | 整程序结构化 HIR/CFG、六个静态 pass、有界整数 MIR、guard/deopt、局部 List 标量替换 |
| Web / Database | 继承 Web Runtime；Database 1.0.0 独立版本，最低要求 HHY 1.5.0 |
| 平台 | macOS arm64、Linux arm64/x86_64；Windows x86_64 为 MSYS2 归档，包内不含 Database |
| 默认准入 | 新 Compiler 与 Runtime 实验仍显式启用；正确性及发行验收通过不表示真实负载普遍加速 |

v1.6.0–v1.6.3、v1.7.0–v1.7.3 在设计资料中表示工程子阶段，统一随产品 **1.7.0** 交付。不能据此查找或安装不存在的独立子阶段发行版。

## 从源码验证执行路径

以下命令在项目根目录运行，使用仓库现有示例。构建依赖与安装方式见 [INSTALL](../INSTALL.md)。

```sh
make
./build/hhy --version
./build/hhy check examples/09-profile-algorithms.hhy
./build/hhy run --engine bytecode examples/09-profile-algorithms.hhy -- fibonacci 20
./build/hhy run --engine ast examples/09-profile-algorithms.hhy -- fibonacci 20
HHY_COMPILER=ir ./build/hhy run --engine bytecode examples/09-profile-algorithms.hhy -- fibonacci 20
```

小示例用于验证命令与执行路径，不用于证明优化提速。查独立完整语法示例：

```hhy
let values = [1, 2, 3]
values |> map { value -> value * 2 } |> collect |> print
```

## 开关与回退

| 开关 | 默认 | 启用后的作用 |
| --- | --- | --- |
| `HHY_COMPILER=ir` | 直接 Bytecode | 选择整程序 HIR → pass → Bytecode 编译链 |
| `HHY_COMPILER_DISABLE=all` | 不禁用 pass | 在 IR 模式关闭全部静态 pass；不关闭 IR/verifier 或独立的 MIR 特化 |
| `HHY_COMPILER_DISABLE=fold,dce` | 不禁用 pass | 关闭指定 pass；完整六项名称见 [Compiler IR](architecture/COMPILER_IR.md) |
| `HHY_COMPILER_REPORT=1` | 关闭 | IR 编译报告写到 stderr，包含 pass 与回退信息 |
| `HHY_FEEDBACK_SPECIALIZATION=1` | 关闭 | 在 Bytecode 编译边界生成整数 MIR，Runtime 依据实际参数反馈与 guard 选择执行；直接编译器也可使用 |
| `HHY_SCALAR_REPLACEMENT=1` | 关闭 | 配合 MIR 特化处理符合条件的直接索引局部 Int List |
| `HHY_BYTECODE_CALL_PLANS=1` | 关闭 | 可选调用布局 |
| `HHY_BYTECODE_EXCEPTION_TABLES=1` | 关闭 | 可选异常区域表 |
| `HHY_CALL_FRAME_POOL=bounded` | `legacy` | 有界 Env 帧池 |
| `HHY_CALL_FRAME_UNWIND=1` | 关闭 | 逻辑调用帧展开登记 |
| `HHY_MAP_INLINE_CACHE=1` | 关闭 | 带当前键校验的单态 Map slot 提示 |
| `HHY_PROFILE_DISPATCH=1` / `HHY_PROFILE_LOOKUPS=1` | 关闭 | profile 命令采集 dispatch / lookup 诊断；采样与无画像计时分开 |

取消上述实验环境变量后重新启动进程，即恢复既有配置。常驻 Web/嵌入式应用也需重新启动或重新创建相关上下文；不要假设修改 shell 环境会改变已经运行的 Worker。

```sh
unset HHY_COMPILER HHY_COMPILER_DISABLE HHY_COMPILER_REPORT
unset HHY_FEEDBACK_SPECIALIZATION HHY_SCALAR_REPLACEMENT
unset HHY_BYTECODE_CALL_PLANS HHY_BYTECODE_EXCEPTION_TABLES
unset HHY_CALL_FRAME_POOL HHY_CALL_FRAME_UNWIND HHY_MAP_INLINE_CACHE
unset HHY_PROFILE_DISPATCH HHY_PROFILE_LOOKUPS
./build/hhy run --engine bytecode examples/09-profile-algorithms.hhy -- fibonacci 20
```

需要语义对照时显式使用 `--engine ast`。`HHY_COMPILER_DISABLE=all` 只关闭静态 pass，不等于恢复直接 Bytecode 编译器。

## 观测与故障定位

```sh
./build/hhy bytecode --metrics examples/09-profile-algorithms.hhy
HHY_COMPILER=ir HHY_COMPILER_REPORT=1 ./build/hhy bytecode --metrics examples/09-profile-algorithms.hhy
HHY_COMPILER=ir HHY_FEEDBACK_SPECIALIZATION=1 ./build/hhy profile --heap --format json examples/09-profile-algorithms.hhy -- fibonacci 20
```

| 现象 | 检查与处理 |
| --- | --- |
| 开启 IR 后没有提速 | 核对是否实际进入候选路径；比较 prepare 在内的端到端耗时，I/O 或不支持形态可能主导成本 |
| `size_fallback` | 候选代码超过尺寸预算，系统整体回退；保留报告与最小输入，不直接放宽预算 |
| MIR hits 为零 | 检查函数形态、连续整数调用热身、类型变化、捕获/动态调用及反馈槽冲突；开关开启不保证命中 |
| 标量替换后堆用量未降 | 当前保留同尺寸托管分配预约，这是维护 GC/配额语义的既定边界 |
| 执行结果或 Error 不一致 | 保留原始输入、版本、平台、开关、stdout/stderr/退出码；关闭具体实验并与 AST 对照 |
| `SKIP[HHY_CAP_*]` | 宿主缺少必要能力；在具备能力的环境补验，不能把 skip 记为通过 |

`bytecode --metrics` 将 JSON 写到 stdout；`profile --format json` 将报告写到 stderr，程序自身 stdout 保持原输出。采集时分开保存两路输出，避免把程序文本当作 JSON 解析。

## 验证与证据口径

修改运行时或编译器时按变更范围执行已有门禁：

```sh
make test-compiler
make test
HHY_COMPILER=ir make test
HHY_COMPILER=ir make test-debug
make test-vm-call-policy
make test-runtime-resources
```

以上为维护时的验证入口，本次文档整理不代表重新执行了这些完整门禁。发布前仍需跨平台 CI、发行包和扩展验收；本地通过不能替代它们。

当前发布依据：[1.7.0 发行说明](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.7.0)、[本地最终发布归档](https://github.com/hh696-wq/hhy-vm/actions/runs/34199886300)、[Compiler 版本化策略](../benchmarks/vm-compiler-ir-policy.json)。真实负载未达到默认启用收益门槛；RDS 实机和 24 小时长稳仍待验证。完整边界见[已知限制](KNOWN_LIMITATIONS.md)。

返回[文档中心](README.md)。
