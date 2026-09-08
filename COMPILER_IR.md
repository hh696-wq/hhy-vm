# v1.7 Optimizing Compiler：整程序本地实施

本阶段接通 `Source → AST → Structured IR → verified passes → Bytecode → VM`。
实现采用一个版本化、非 SSA 的结构化 IR，不额外制造 HIR/MIR 两套中间表示。
`HHY_COMPILER=ir` 启用整程序编译链；原 AST 引擎与直接 Bytecode 编译器保留为独立对照。
默认仍使用直接 Bytecode 编译器。默认准入需要各平台自己的编译成本、代码尺寸、真实运行和资源证据。
这里的 v1.7 是实施阶段编号，`VERSION` 保持 1.5.0，不创建 tag、Actions 或 release。

## 四个子阶段的交付边界

| 子阶段 | 实施交付 | 准入边界 |
| --- | --- | --- |
| v1.7.0 | 整程序 IR、区域/CFG、独立 verifier、文本及 JSON dump、IR→Bytecode 后端 | 所有已有语法保留；动态操作保留 Runtime 检查；不引入 SSA/phi 或 JIT |
| v1.7.1 | 六个独立 pass：fold、constant-propagation、copy-propagation、peephole、unreachable、dce | 仅证明安全的规则变换；代码尺寸增长时整个候选回退 |
| v1.7.2 | 调用目标反馈评估、保守聚合值逃逸分类、特化/去装箱准入决策 | 尚缺参数类型反馈、guard 生命周期证明及跨平台收益，保留通用调用与托管分配 |
| v1.7.3 | pass timing、IR/Bytecode 大小、开关/原因报告、差分/变形/变异/fuzz、预算脚本与本地记录 | 跨平台默认准入与发布没有执行，不把单机或合成样例收益外推 |

高级优化的交付是有证据的条件决策，不是宣称已实现 speculative specialization、deopt、标量替换或“缓存/逃逸无收益”。
源码入口是 `include/hhy/compiler.h`、`src/compiler.c`；原 `compiler/ir.c` 是保留用于回归的闭合 I64 研究原型，不承担整程序执行。

## IR 表示与 verifier

`HHY_COMPILER_IR_VERSION=1` 使用有界 node arena 和每节点两个 CFG 槽位。节点保留原 token、源码位置/长度、父子/兄弟关系、槽位布局、类型、常量值与 effect 位。
四个 effect 位分别是 throw、cancel、allocation、external effect。只有短且合法的整数、Bool/null 及成功的有限标量运算证明为零 effect；未知节点保守标记全部 effect。
这不声称每个未知操作实际都会产生全部 effect，而是禁止依赖未知行为做删除或重排。

CFG 的 block 是结构化语句操作；表达式是按语言顺序求值的嵌套区域。包括 if 的两条边、while 回边、for 首次求值与 advance 两阶段、break/continue、return、try/catch 绑定入口和函数/闭包独立 owner。
调用、短路、attempt 等表达式内部语义仍封装在该结构化操作中；异常边是保守摘要，不能拿它证明未知调用无异常。`UINT32_MAX` 表示当前函数/程序的出口或不存在的边。
不进行 loop unrolling、循环不变量外提、跨调用/循环的值传播或 SSA phi 合并。

Verifier 可独立调用，不依赖先执行某个 pass。它检查：

- 版本、容量、opcode、arity、前向索引、区域唯一所有权、嵌套上限和源码元数据；不可达 arena 槽位也先做安全结构检查；
- 重算类型、常量值与 effect，拒绝降低 effect 或伪造派生常量；CONST 是 IR 自身的合法定义；
- 重建语句控制流、循环阶段、exception 和 callable owner，拒绝缺失或篡改的边；
- 传播结果记录 immutable LET 来源，校验同一区域中最近的先前定义及 copy 链值；不允许自引用或越过 shadowing；
- 独立重建后端视图并重新解析局部槽位，核对每个直接槽位 use 的作用域、定义顺序和 slot；未解析的动态名字继续走 Runtime 检查；
- 每个启用的 pass 后重新 verify；发射前再次 verify；Bytecode 发射后由既有独立 verifier 检查结构、Kernel 栈、调用布局与异常表。

这是 memory-form/structured IR 的定义与作用域验证，不宣称实现通用 SSA dominance/phi verifier。
接口接收具有有效 C 对象/存储生命周期的内部对象，初始化必须为零；没有外部不可信 IR 二进制反序列化入口。

## 后端、资源与错误保持

IR 后端根据自己的节点构造临时、独立拥有的 emitter 视图，交给既有直接 Bytecode emitter。它不复用或修改输入 AST，也不重新解析源码。
临时视图和 IR 在编译完成后释放；Runtime 执行的是 chunk，不在执行期间补做优化 pass。模块编译也经过同一个编译选择入口。

保留原 frame capacity、声明、绑定、Env、闭包捕获、容器/字符串/Float 分配、调用、循环 header 和取消点。
已证实不会执行的分支体/终止语句后的代码可以删除；条件和循环 header 本身仍保留。
保留 block 的末尾值表达式，避免改变隐式返回。DCE 不缩减闭包的顶层 body 序列，防止意外改变既有快速调用入口选择。

整数求值使用 checked add/sub/mul，拒绝折叠溢出、除零与 `-INT64_MIN`；`INT64_MIN % -1` 保留 0。
负数通过原有 unary/binary 指令表示，不能扩大局部指令数；极值无法经济表示时保留原表达式。
`/`、Float/单位/String、未知类型、长整数文本及大整数相等比较的边界均保守保留，避免改变分配、舍入或既有路径行为。

错误仍发生在原运算/调用位置。成功消除的运算会减少 dispatch 次数，这是优化的观测结果；函数、Error 和调用栈的源码归因仍保留。
错误文本/位置、退出码、输出、副作用、资源限制和取消另外做差分验证，不能用“只打印相同结果”替代。

## 六个 pass

| 开关名称 | 规则 | 保守拒绝 |
| --- | --- | --- |
| `fold` | 成功的闭合 Int64、Bool/null 标量结果 | 溢出/除零、未知值、分配、后端指令增长 |
| `constant-propagation` | 同一顺序区域的不可变已知标量绑定 | mutable、跨作用域/调用控制流、负数 load 展开 |
| `copy-propagation` | 追溯同一区域已知不可变标量的复制链 | 动态别名、类型未知、shadowing、未证明来源 |
| `peephole` | 已证明 Int 的 `+0`、`-0`、`*1` 与 unary `+` 合并 | 不能证明类型/无错误，尤其不能把未知 `x+0` 当恒等式 |
| `unreachable` | return/break/continue 后缀、确定条件的未执行分支体 | 保留 condition、loop header 和作用域容量 |
| `dce` | 删除非末尾、零 effect 的纯值语句 | 任何可能 throw/cancel/allocate/effect，末尾结果和声明 |

执行顺序为 copy propagation → constant propagation → fold → peephole → unreachable → DCE；初始元数据已经计算可证明的标量值。
每项可独立关闭，关闭某个 pass 不会关闭另一个 pass 自己的安全证明，例如 DCE 可以独立证明一个纯算术表达式无效。

```sh
HHY_COMPILER=ir build/hhy run program.hhy
HHY_COMPILER=ir HHY_COMPILER_DISABLE=all build/hhy run program.hhy
HHY_COMPILER=ir HHY_COMPILER_DISABLE=fold,dce build/hhy run program.hhy
HHY_COMPILER=ir HHY_COMPILER_REPORT=1 build/hhy bytecode --metrics program.hhy
make build/hhy-compiler-probe
build/hhy-compiler-probe --text program.hhy
build/hhy-compiler-probe --json program.hhy
build/hhy-compiler-probe --metrics program.hhy
```

`HHY_COMPILER_REPORT=1` 在 stderr 输出每次编译报告；默认不污染程序输出。JSON dump 包含 before/after、active 与原始 arena 节点，以及 CFG/source/provenance。
`changed` 对 fold/传播/peephole 是变换数，对 unreachable/DCE 是删除节点数。`ir_bytes` 是 arena/边/token 的保留字节，不是进程峰值，也不会因逻辑删除自动降低。
`emit_verify_ns` 包含后端发射、验证及直接 Bytecode 尺寸参照的编译；pass timing 包含对应 verifier，时钟量化可能出现零。

## 条件反馈与逃逸决策

`scripts/evaluate-compiler-feedback.py` 采集稳定/变化调用目标和真实聚合值用法。
现有反馈只观察目标身份/转换，不能证明参数类型稳定；即使一个 site 在本次运行单态，也不能忽略后续重绑定、模块/闭包生命周期和 guard fallback。
因此本次不生成新的推测执行代码，关闭编译优化即回到通用 Runtime；没有虚构 deopt 已实现。

逃逸分析只把直接作为 index 接收者的 List/Map 标为局部候选，其余赋值、返回、调用传递和未知用法保守视为可能逃逸，闭包全部保留捕获关系。
即使局部候选也有可观察的 allocation quota/错误行为，不能直接删除托管分配。没有更换 Value 表示、GC root、write barrier 或生命周期策略。
这些限制是机器可读拒绝原因，不是“没有性能收益”的结论。

## 本地验收与跨平台预算

```sh
make test-compiler
HHY_COMPILER=ir make test
HHY_COMPILER=ir make test-debug
make fuzz-smoke
HHY_COMPILER=ir python3 scripts/run-bytecode-workloads.py --output build/compiler-workloads.json
make build/hhy-resource-probe
python3 scripts/evaluate-compiler-program.py --output build/compiler-budget.json
python3 scripts/evaluate-compiler-feedback.py --output build/compiler-feedback.json
```

新增验收进入 `make test` / `make test-debug`；语法 fuzz 同时覆盖真实 IR 链路。
测试包含固定种子表达式、64 个开关组合、整程序三类编译路径、源码错误/stack、额度边界、非法 CFG/effect/类型/直接槽位/传播来源、10,000 次有合法 C 存储的字段变异。

候选 Bytecode 的 instruction 数和逻辑存储字节（包含常量、Kernel、调用/异常表）不得超过直接 Bytecode，超出就整体回退并报告 `size_fallback`。
测量预算采用 compile ≤ 2×直接编译 + 500μs、Runtime ≤ 95%、分配 ≤ 101%；保留原始样本和机器信息。Runtime 测量包含 prepare，而不是只计预编译后的热循环。
不能用 synthetic 算术循环收益抵消真实负载回归；macOS arm64 单机数据不代表 Linux/Windows 或任一其他平台通过。
版本化策略见 `benchmarks/vm-compiler-ir-policy.json`。详细结果见本地 `performance-analysis/2026-09-08-v1.7-complete-local/DEVELOPMENT.md`。
