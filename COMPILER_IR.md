# v1.7：独立 IR 研究原型

按继续推进的要求，开始独立 Compiler 规则验证；此前性能数据不足以准入优化的结论不变。
本轮不是完整 v1.7.0，更不是完整 v1.7。原型放在 compiler/，不进入生产 SOURCES，不改变 AST/Bytecode 默认执行路径。

## 边界决策

先验证一个 IR 层，不预建 HIR/MIR 两套表示。当前只有一个 I64 basic block、顺序定义和 RETURN，最多 256 条指令。
仅支持单条 `print(expression)` 中的闭合十进制整数表达式：常量、加减乘、取余和取负。print 本身不进入 IR。
浮点、整数除法（HHY `/` 的类型语义不同）、带分隔符/进制的字面量、变量、调用、控制流、模块和整个真实项目均不在子集内。
拒绝这些输入是原型范围限制，不能解释成它们没有优化价值或实际项目没有可优化表达式。

## Verifier 与 source/effect 规则

IR v1 的所有值为 I64；CONST/算术定义值，RETURN 结束唯一基本块。没有外部输入寄存器、phi、边或循环。
独立 verifier 检查版本、block/entry/count、唯一且末尾的 terminator、合法 opcode、操作数必须引用先前定义、无用 immediate/operand 的规范值、source line/column/token length 和 effect flags。
因此可以证明本子集中的定义先于使用；不能称作一般 CFG dominance/loop/phi 验证。source 目前是 operator/literal token 位置，不是完整多文件 source-span 验证。

所有算术均显式 may_throw；CONST/RETURN 不 throw。当前抽象 IR 的 may_cancel/allocates/external_effect 为 false。
这些是独立整数值计算模型的属性，不是对现有 Runtime 每个 AST 节点的资源行为断言。Runtime literal 处理、调度检查与可观察资源计数尚未映射，不能据此删除 Runtime 分配或取消点。

只接受解析器产生的 AST，以及已具有合法 C 字段表示的内部 HhyIR 对象。没有外部 IR 反序列化入口；随机原始字节不是合法 C bool/enum 对象。未来若导入序列化 IR，必须在构造类型前验证格式和值域。

## 折叠 pass

`--no-fold` 独立关闭，IR 原样复制。`--fold` 在 verify 后用 checked I64 运算求值：

- 全部成功：替换为 CONST + RETURN，保留表达式结果的 operator source 位置；
- 溢出、取负溢出或除零：整段原样保留，不抢先产生编译错误，不改变第一个异常位置；
- INT64_MIN % -1 保持 HHY 的 0 结果；不调用会触发 C 未定义行为的除余操作；
- 非法 IR：拒绝并保持输出对象不变；优化后再次独立 verify。

这不是常量传播、DCE、copy propagation、peephole、一般 CFG 优化、feedback specialization 或 escape analysis。
原型没有 IR→Bytecode 后端，不替换 Runtime。异常测试核对错误类别/消息与首个源码位置，但没有证明完整 Error 对象、stack trace、取消延迟和资源计数等价。

## 工具与本地验证

```sh
make test-compiler-ir
make build/hhy-ir-probe-debug build/hhy-ir-test-debug
build/hhy-ir-test-debug
python3 tests/check-compiler-ir.py build/hhy-ir-probe-debug
build/hhy-ir-probe --no-fold expression.hhy
build/hhy-ir-probe --fold expression.hhy
build/hhy-ir-probe --text expression.hhy
python3 scripts/evaluate-compiler-ir.py --output build/compiler-ir.json
```

JSON 输出 before/after IR、验证结果、显式拒绝原因和时间；文本输出寄存器、操作、操作数、immediate 与源码位置。HHY_IR_NONE=UINT32_MAX 表示不存在操作数。
Release 和 ASan/UBSan 验证包括 127 个固定种子/边界表达式 × fold 开关 × AST/Bytecode 对照、7 类子集外输入，以及各 10,000 次合法 C 字段表示下的 IR 变异。
错误表达式的 before/after 整段一致，用于验证不能以折叠改变第一处错误。

成本脚本每例 2 次预热、15 次交替开关测量。lower_verify_ns / fold_verify_ns 包含对应 verifier；零值可能来自时钟量化，不代表零开销。
墙钟时间包含原型进程启动和 JSON 序列化，不是 Runtime 执行收益。instruction 数量可减少，但固定 256 槽 IR 容器占用不会随折叠下降；reserved_ir_bytes 与逻辑 code bytes 分开理解。

## 后续准入

完整 v1.7.0 仍需要通用 control-flow/source/effect/allocation/cancellation 模型、IR→Bytecode 后端及整程序三路径矩阵。
v1.7.1 的 Runtime pass 需要副作用/错误顺序、资源/取消/stack/profiler 等价，以及编译耗时、代码尺寸、真实运行收益预算；当前均未获准入。
跨平台性能、反馈特化、逃逸分析与 JIT 未启动。机器决策见 benchmarks/vm-compiler-ir-policy.json。
