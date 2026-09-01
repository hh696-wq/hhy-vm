# HHY 下一阶段详细迭代路线图

日期：2026-08-31

当前实施版本：1.2.1

适用范围：Language、VM/Runtime、CLI、编辑器、扩展协议、扩展分发、实战项目与生态开放

## 1. 路线结论

HHY 已经完成核心语言语义、AST evaluator、Flow/Stream、系统 I/O、有界并发、资源限制、Profiler、本地进程扩展、安全 Spider 和可恢复抓取。下一阶段不应无边界增加语法或 Runtime builtin，而应依次完成：

```text
v1.1.x  稳定基线、诊断体验与低风险架构治理
    ↓
v1.2.0  官方扩展分发与签名
    ↓
v1.2.1  锁定、离线、可复现安装与回滚
    ↓
v1.2.2  官方复杂扩展验证
    ↓
v1.2.3  根据真实证据补强进程协议
    ↓
v1.3    真实用户与旗舰场景
    ↓
v1.4    有条件的 AST → Bytecode VM 演进
    ↓
v2.0    有条件的生态开放与 ABI 决策
```

基本原则：

1. 已冻结的 Pipe、Value、Stream、Error 和取消语义不因排期改变；
2. 先建立可用、可测、可回滚的能力，再追求性能和更大的生态表面；
3. 第三方能力优先通过 Process Extension Protocol 接入，不另造语言语义；
4. Native ABI、FFI 和 Bytecode VM 必须由真实证据触发。

## 2. 版本总表

| 版本 | 状态 | 核心目标 | 关键交付 | 进入下一阶段的发布门槛 |
| --- | --- | --- | --- | --- |
| v1.1.6 | **已完成 · 2026-08-31** | 稳定基线与测试治理 | CI 分层、宿主能力探测、性能基线、发布清理 | 正式平台 CI 全绿；测试无模糊失败；基准可重复 |
| v1.1.7 | **已完成 · 2026-08-31** | 诊断与编辑器基线 | JSON diagnostics、最小 LSP、Contract 感知补全 | CLI/LSP 诊断一致；中型项目具备编辑闭环 |
| v1.1.8 | **已完成 · 2026-08-31** | Runtime 渐进治理 | 首个模块边界、内部所有权 API、性能回归门禁 | 行为零变化；sanitizer/GC stress 全绿；性能不回退 |
| v1.2.0 | **已完成 · 2026-08-31** | 官方扩展分发与签名 | 包身份、Ed25519 签名、依赖解析、静态官方索引、事务式安装 | 来源和依赖可验证；篡改包拒绝；失败安装不破坏现有环境 |
| v1.2.1 | **已完成并发布 · 2026-09-01** | 锁定、离线与安全回滚 | Lockfile、离线缓存、可复现安装、事务式升级/回滚 | 同一 lock 得到同一依赖图；失败升级不破坏旧环境 |
| v1.2.2 | 规划 | 官方复杂扩展验证 | Office 或等价复杂扩展、端到端发行与兼容验证 | 至少一个复杂扩展通过三平台、权限、错误和资源验收 |
| v1.2.3 | 条件规划 | 证据驱动的协议补强 | 按需加入取消、Stream credit、Opaque Handle | 每项新增能力均由真实集成和兼容测试证明 |
| v1.3 | 规划 | 旗舰场景与外部采用 | 模板、CI 集成、运维文档、3–5 个外部案例 | 外部用户可独立完成真实任务并形成可归类反馈 |
| v1.4 | 条件规划 | AST → Bytecode VM | 保留语言语义，把 AST lowering 为可验证 Bytecode，并建立新执行引擎 | 真实负载证明 AST dispatch 是主瓶颈；双引擎语义一致 |
| v2.0 | 条件规划 | 生态开放与 ABI 决策 | 进程协议边界报告、Embedding/FFI/Native ABI 决策 | 至少两个真实集成证明进程协议不足，否则不开放 ABI |

总表只列稳定版本和 release train，避免把开发阶段误解为多个并列产品版本。v1.4 的交付阶段单列如下：

| v1.4 阶段 | 阶段目标 | 关键交付 | 退出门槛 |
| --- | --- | --- | --- |
| v1.4.0-alpha | Bytecode 设计与编译器骨架 | Chunk、Opcode、常量池、源码位置表、AST compiler、Verifier、反汇编 | 核心语法可编译；非法 Bytecode 可拒绝；AST 仍为默认引擎 |
| v1.4.0-beta | Bytecode VM 执行核心 | Operand stack、CallFrame、Closure/Upvalue、异常、GC roots、取消和资源限制 | 核心 fixtures 通过 AST/Bytecode 双引擎语义对照 |
| v1.4.0-rc | 性能与默认切换评估 | 真实 benchmark、Profiler、Stack trace、三平台和故障注入 | 语义零差异；CPU 负载有实质收益；I/O 负载无明显回退 |
| v1.4.0 | Bytecode 正式发布 | 达标时默认启用 Bytecode，保留 `--engine ast` 回退 | 至少一个 RC 周期稳定；诊断、资源安全和跨平台证据完整 |
| v1.4.1 | 真实负载兼容加固 | 修复现场差异、性能回退和可观测性问题 | 双引擎测试持续通过；无新增高优先级正确性问题 |
| v1.4.2 | VM 稳定化 | 固化 compiler/VM 内部边界，持续保留 AST 语义 oracle | 多版本无语义漂移；AST 对照测试持续运行 |

说明：官网的“v1.2 官方扩展包分发与工具链”是整个 v1.2 release train 的总目标。v1.2.0 建立安全分发主链路和最低限度的事务式安装；v1.2.1 补齐离线锁定、可复现安装和显式回滚，至此完成公开 v1.2 目标闭环。v1.4 只有满足启动条件才进入 alpha；alpha、beta、rc 是质量门禁，不是可以按日期强行发布的功能版本。

## 3. v1.1.x：稳定基线、诊断体验与 Runtime 治理

v1.1.x 不扩大语言和生态承诺。原先考虑放入 v1.2 的 LSP、结构化诊断和 Runtime 渐进拆分前移到 v1.1.x，避免覆盖官网公开的 v1.2 扩展工具链定位。

### 3.1 v1.1.6：稳定基线与测试治理

> **状态：已完成（2026-08-31）**<br>
> 实施提交：`b1f099abd798f625a99b23e44f2173bc13bbfe40`<br>
> GitHub Actions：[HHY v1.1 release evidence #33350901625](https://github.com/hh696-wq/hhy-vm/actions/runs/33350901625) · [HHY website #33350901628](https://github.com/hh696-wq/hhy-vm/actions/runs/33350901628)

#### 版本目标

把 v1.1.5 之后的代码、测试、文档、网站和发布证据整理为干净、可信、可重复验证的基线。

#### 具体内容

| 领域 | 工作项 | 交付物 |
| --- | --- | --- |
| 工作树治理 | 区分源码、测试证据、推广素材和生成物 | `.gitignore`、生成物目录约定、证据归档规则 |
| 测试能力探测 | 探测 `/bin/ps`、socket bind、网络、文件监听和可选工具 | 统一 capability probe；每个 skip 输出稳定 reason code |
| CI 分层 | Core、Extensions、Practical Projects、Website、Release Evidence 分开执行 | 独立 job、失败归属清晰、必要 job 受保护 |
| 性能基线 | 固定启动、Flow、Map、Codec、HTTP、parallel 和 Heap 基准 | 可机器比较的 JSON benchmark report |
| 发布治理 | 固定版本、包内容、校验和、依赖与三平台证据 | RC checklist、自动一致性检查、发布清单 |
| 文档一致性 | 检查 README、官网、规范、限制和版本号 | 文档代码块、链接、版本一致性 CI |

#### 已交付的性能基线

- CLI `--version` 启动基线；
- 代表性基础 Flow 执行基线；
- 10 万项 `map`、`where`、`distinct`、`collect` 核心 Flow；
- JSON Flow 解析与投影；
- JSON 报告记录 schema、HHY 版本、commit、平台、架构、Python、CPU 数量及不少于五次样本；
- GitHub Actions 保存 30 天的跨提交 performance baseline artifact。

以下扩展基准移入 v1.1.8 的性能门禁建设，不作为 v1.1.6 完成状态的虚假声明：宽 Map 分级规模、CSV、HTTP 边界、parallel worker、Heap 峰值与跨提交回退阈值。

#### 发布门槛

- [x] macOS arm64、Linux arm64、Linux x86_64 CI 全绿；
- [x] Windows MSYS2 核心 smoke 通过；
- [x] 宿主能力不足产生 `SKIP[HHY_CAP_*]` 明确 reason code；
- [x] benchmark 连续运行五次，记录中位数、原始样本和环境；
- [x] 产物、版本、SHA-256、BUILD_INFO、文档和官网信息一致；
- [x] Release、Debug ASan/UBSan、GC stress、fuzz、Practical Projects 和 Website 验收通过；
- [x] 未改变 Pipe、Value、Stream、Error、退出码或 Process Extension Protocol 语义；
- [x] 没有创建 Git tag、GitHub Release 或正式发布包；当前仅完成开发基线与 CI 证据。

#### 完成证据

| 验收项 | 结果 |
| --- | --- |
| Quality、Docs、Benchmark | 通过，performance artifact 已上传 |
| Windows x86_64 MSYS2 | 构建、Parser 和 Runtime smoke 通过 |
| Linux x86_64 | UBSan、Release suite、coverage fuzz、归档与校验通过 |
| Linux arm64 | ASan/UBSan、Release suite、coverage fuzz、归档与校验通过 |
| macOS arm64 | ASan/UBSan、Release suite、fuzz smoke、归档与校验通过 |
| Practical Projects | my-crawler 与 SiteGraph Auditor 验收通过 |
| Website | HHY 示例、ESLint、TypeScript、Next.js production build 通过 |
| 本地受限环境 | `/bin/ps` 与 socket bind 被明确识别并跳过，完整 suite 最终通过 |

备注：工作区内原有、与 v1.1.6 无关的推广截图和历史评审目录被刻意保留为未跟踪文件，没有被删除或混入实施提交；“发布清理”在本版本中指实施变更、构建输出和正式提交边界可审计，而不是擅自删除用户素材。

### 3.2 v1.1.7：结构化诊断与最小 LSP

> **状态：已完成（2026-08-31）**<br>
> VS Code 扩展：0.2.0<br>
> 发布策略：只提交开发版本与 CI artifact，不创建 Git tag 或 GitHub Release

#### 版本目标

让用户在编辑器和 CI 中顺畅编写、检查和修复中型 HHY 项目，不依赖 `tokens`、`ast` 或阅读 Runtime 源码定位普通错误。

#### CLI 与诊断

- `hhy check --format json` 输出版本化稳定 schema；
- 诊断包含 path、零基 line/character range、severity、稳定 code、message 和 stage；
- 保留默认文本诊断，JSON 模式捕获同一次 Core Parser/Checker 执行结果；
- `hhy contracts --format json` 暴露同一 Callable Contract Registry；
- JSON diagnostics 与 contracts schema 均以 `schema_version: 1` 版本化；
- 多文件检查聚合到一个机器可读报告，失败继续保持退出码 2。

#### 最小 LSP

- publishDiagnostics；
- 当前文档本地 binding 的 go to definition；
- Hover 展示 callable input/output、effect、lazy、cancellable 和 threading；
- 从 Core Contract Registry 加载 callable completion；
- document formatting / format on save；
- full-document sync 与 didOpen/didChange/didClose；
- VS Code 0.2.0 使用标准 `vscode-languageclient` 接入；
- `hhy.executablePath` 支持不在 `PATH` 的 HHY CLI；
- standalone `editors/lsp/server.mjs` 可供其他 LSP 客户端接入。

#### 发布门槛

- [x] CLI 文本诊断和 LSP diagnostics 来自同一次 Core 检查路径；
- [x] JSON diagnostics 与 Contract Registry schema 有兼容断言；
- [x] LSP diagnostics、definition、Hover、completion、formatting 有协议测试；
- [x] VS Code extension 通过固定依赖、esbuild bundle 和 VSIX 构建验证；
- [x] LSP 作为独立进程，故障不影响 CLI/Runtime；
- [x] 合法 v1.1 程序执行结果不变；Release 与 Debug suite 通过；
- [x] 不创建 Git tag、GitHub Release 或正式发行包。

#### 后续增强，不计入 v1.1.7 完成声明

- document symbols、workspace symbols 和跨模块 definition；
- 从已加载第三方扩展动态刷新 completion；
- 未使用 binding/import、Result 未处理和 Stream 重复消费等新 lint；
- callable 拼写修复、Pipe 注入参数可视化和 code action；
- Sublime 原生客户端配置与更多编辑器发行包。

这些能力需要新的 Checker/Workspace index 或扩展生命周期设计，不能为了把“最小 LSP”包装成完整 IDE 而在 v1.1.7 中虚假承诺。

### 3.3 v1.1.8：Runtime 渐进拆分与性能门禁

> **状态：已完成（2026-08-31）**<br>
> 发布策略：只提交开发版本与 CI artifact，不创建 Git tag 或 GitHub Release

#### 版本目标

建立可持续拆分所需的边界、所有权语言和自动门禁，同时保持公开行为、默认资源限制、性能和内存安全不变。本版本不宣称一次性完成整个 Runtime 拆分。

#### 拆分顺序

```text
Value / Equality / Hash / Map
        ↓
Stream core / Stream operators
        ↓
JSON / CSV / Codec
        ↓
Filesystem / Process / HTTP
        ↓
Evaluator / Runtime owner / Cleanup
```

#### 具体内容

- [x] 将默认资源限制策略抽到 `runtime_limits.c`，形成第一个行为零变化模块边界；
- [x] 为 borrowed、managed-scanned、managed-atomic、native-owned 制定内部注解；
- [x] 为 scanned/atomic allocator、数组增长和 Runtime owner teardown 加所有权标注；
- [x] 静态检查禁止 managed `Value` 数组误用 native allocator；
- [x] `docs/RUNTIME_GOVERNANCE.md` 固化模块责任和逐职责迁移顺序；
- [x] benchmark 进入 `make quality` 阻断门禁，检查绝对预算和启动归一化比率；
- [x] 性能预算独立版本化，修改必须提供显式评审与 benchmark 解释；
- [x] Runtime owner 继续统一关闭 Stream、Module 和 Cleanup，未新增公开 ABI。

Value/Map、Stream、Codec 和 effectful resource 的物理拆分保留为后续逐步迁移项；只有每一步均满足本节门禁才允许合并，避免以“大爆炸重写”冒充治理完成。

#### 发布门槛

- Release、Debug、ASan/UBSan、GC stress 和 fuzz smoke 全绿；
- `.hhy` fixtures 输出和退出码保持一致；
- 资源上限、取消和 close 幂等保证不降低；
- benchmark 无未经解释的显著回退；
- 每次只迁移一个稳定职责，禁止一次性重写；
- 不引入公开 C ABI。

## 4. v1.2：官方扩展包分发与工具链

v1.2 保持官网公开定位：让官方扩展具备可信来源、依赖解析、远程发现、离线锁定、可复现安装和安全回滚。它不是语言语法版本，也不是 Native ABI 版本。

### 4.1 v1.2.0：分发、发布者身份与签名

> **状态：已完成（2026-08-31）**<br>
> 实施提交：`8e81ca1c909901667f7f7d03c7e3ccc6f1349528`，跨平台修复截至 `4b3f9b0599adcd625ed1307ea74cbbf7ae7ca120`<br>
> GitHub Actions：[HHY release evidence #33356076638](https://github.com/hh696-wq/hhy-vm/actions/runs/33356076638) · [HHY website #33355924739](https://github.com/hh696-wq/hhy-vm/actions/runs/33355924739)<br>
> 已交付：静态签名 Registry、显式 Ed25519 信任根、索引与包描述双层签名、同版本多 target 产物选择、确定性传递依赖解析、全图校验、`--dry-run`、同文件系统 staging、原子单包提交和失败图回滚。未创建 Git tag、GitHub Release 或正式发布包。

#### 版本目标

从“只能安装本地路径”升级为“可以从官方远程索引发现、解析并验证扩展”，同时保留本地路径开发安装。

#### 包与索引模型

- 定义远程索引 schema 和 schema version；
- 包坐标包含 publisher、name、version 和 target；
- 支持语义版本及 HHY/Protocol 兼容范围；
- 索引记录 URL、大小、SHA-256、签名、依赖、capability 和平台；
- 索引与包内容分别签名；
- 支持索引缓存、过期策略和显式刷新；
- 本地与远程安装复用同一 manifest 校验路径。

#### 发布者和签名

- 定义官方发布者身份和信任根；
- 签名绑定 manifest、binary、resources 和目标平台；
- 安装前验证 publisher、签名、SHA-256 和文件清单；
- 区分未知发布者、签名无效、内容篡改、撤销和不兼容；
- 支持密钥轮换与撤销元数据；
- CLI 展示来源、发布者、capability、依赖和文件变更。

#### 依赖解析

- 解析直接和传递依赖；
- 检测冲突、循环依赖和缺失平台产物；
- 解析结果确定性排序；
- 默认拒绝隐式降级；
- 安装前生成完整计划，dry-run 无文件副作用；
- 相同索引快照得到相同依赖图。

#### 最低事务安全

- 下载、验签和解包必须先在 staging 目录完成；
- 完整验证成功后才能原子发布新安装；
- 安装失败不得覆盖或删除当前可用版本；
- 临时目录、中断下载和失败安装可以安全清理；
- v1.2.0 先保证“失败不破坏”，v1.2.1 再提供用户可操作的版本历史与显式回滚。

#### CLI

```text
hhy search <query>
hhy info <publisher/package> [--version <version>]
hhy install <publisher/package> [--version <constraint>]
hhy install <local-path>
hhy update [package]
hhy list
hhy remove <package>
```

#### 安全测试

- 篡改 manifest、binary、resource 或 checksum 时拒绝；
- 拒绝非 HTTPS、不可信重定向和超限下载；
- 解包防止路径穿越、符号链接逃逸和覆盖非包文件；
- 依赖数量、深度、包大小、下载时间均有上限；
- 安装前后验证 capability 与 Protocol version；
- 三平台通过本地 fixture registry 确定性测试；
- 至少一次官方 staging index 端到端验收。

#### 发布门槛

- 官方包可发现、解析、下载、验证和安装；
- 安装可追溯到 publisher、索引快照和 digest；
- 篡改及来源不明包稳定拒绝；
- 依赖冲突具有确定错误和修复建议；
- 下载、验签、解包或启动失败不破坏现有扩展；
- 本地开发安装保持兼容；
- 不承诺第三方开放发布，不开放 Native ABI。

### 4.2 v1.2.1：锁定、离线、可复现安装与回滚

> **状态：已完成并发布（2026-09-01）**<br>
> 固定发布约束：先完成代码和本地验收；随后 Linux、macOS、Windows GitHub Actions 必须全部通过并生成对应发行产物；GitHub Release 永远是最后一步，禁止在任一平台失败或产物不齐时提前发布。
> 实施提交：`0883ffc`（核心能力）、`4e498f3` / `ed8cd00`（GCC 诊断兼容）、`8730845` / `9993074`（四平台 Release 产物与 Windows 可移植验收）<br>
> GitHub Actions：[三平台与架构 Release Evidence #33460776309](https://github.com/hh696-wq/hhy-vm/actions/runs/33460776309) · [四平台正式 Release #33461029986](https://github.com/hh696-wq/hhy-vm/actions/runs/33461029986)<br>
> 正式发布：[HHY Language 1.2.1](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.2.1)；包含 macOS arm64、Linux x86_64、Linux arm64、Windows x86_64 归档、逐包 SHA-256、合并 `SHA256SUMS` 与构建来源证明。

#### 版本目标

补齐官网公开 v1.2 的剩余验收项，使扩展环境可重建、审计、离线部署，并能在失败升级后恢复。

#### Lockfile

- 记录精确版本、平台、digest、publisher、签名身份和依赖边；
- 记录索引快照或等价不可变引用；
- lock schema 显式版本化；
- lock 更新与普通安装分离；
- `--locked` 下解析漂移即失败；
- 跨平台可选依赖不写入宿主偶然状态。

#### 离线与缓存

- content-addressed 包缓存；
- 可提前拉取完整依赖图；
- `--offline` 禁止网络；
- 离线安装仍验证 digest 和签名；
- 检测、隔离和恢复损坏缓存；
- 导出/导入离线 bundle，并保留来源证据。

#### 可复现安装

- 相同 HHY、平台、lockfile 和缓存产生相同扩展集合；
- 安装元数据不含不稳定时间或随机顺序；
- 提供机器可读环境清单和审计输出；
- CI 可验证安装与 lockfile 完全一致；
- 区分“包内容可复现”和“扩展运行结果可复现”。

#### 安全升级与回滚

- 下载、校验、解包在 staging 目录完成；
- 全部成功后原子切换 active version；
- 失败升级保留当前版本；
- 支持回滚到最近已验证版本；
- 删除不能影响其他版本或包文件；
- 回滚/卸载保留不含敏感参数的审计事件；
- 对运行中扩展采用延迟切换或明确拒绝策略。

#### CLI

```text
hhy lock
hhy install --locked
hhy fetch --locked
hhy install --offline --locked
hhy verify
hhy rollback <package>
hhy doctor extensions
```

#### 发布门槛

- [x] 干净机器可凭 lockfile 和离线 content-addressed cache 重建环境；
- [x] 索引漂移、缓存损坏、验签失败和安装失败不破坏旧版本；
- [x] upgrade/rollback 通过事务恢复与审计断言；
- [x] cache、lock、active installation 可由 `doctor` 核对；
- [x] 官网承诺的签名、依赖、索引、离线锁定、可复现安装和回滚形成代码闭环；
- [x] GitHub Actions 上 Linux、macOS、Windows 全部通过并产出相应归档；
- [x] 在上述门禁全绿后执行最后一步 GitHub Release。

### 4.3 v1.2.2：官方复杂扩展验证

#### 版本目标

用真实、复杂、跨平台的官方扩展验证 v1.2 工具链和 Process Extension Protocol，而不是用示例扩展宣布生态成熟。

#### 扩展选择

优先候选为 Office 扩展，因为它能同时验证：

- 大文件和结构化数据；
- 多步骤资源生命周期；
- capability 与文件权限；
- 结构化 Error、取消和超时；
- 潜在 Opaque Handle 或流式结果需求；
- 第三方依赖打包和跨平台发行。

若 Office 真实需求不足，可选择等价复杂度扩展，但必须覆盖相同协议压力面，不能退化为同步 JSON 示例。

#### 功能边界

- 首版只支持明确可测试的读写子集；
- 不引入宏执行或不受控脚本；
- 大数据有硬上限，优先批量投影而非逐单元跨进程调用；
- 副作用经过 capability 与 EffectDispatcher；
- 错误保留扩展名、操作名、stage、cause 和稳定 code；
- 安装、升级、锁定、离线和回滚全部走 v1.2 工具链。

#### 端到端验收

- 三个正式平台运行同一 fixture；
- 从官方索引在线安装；
- 从 lockfile 和离线 bundle 重建；
- 篡改包、依赖和签名时拒绝；
- 中断升级后旧版本仍可运行；
- 资源上限、超时、取消和扩展崩溃产生稳定结果；
- 至少一个真实项目连续使用。

#### 发布门槛

- 复杂扩展证明分发链可用于真实负载；
- 收集调用次数、吞吐、序列化开销、内存和取消延迟；
- 记录协议缺口与可接受规避方式；
- 不因单个扩展的便利性直接公开 Runtime C ABI。

### 4.4 v1.2.3：根据证据补强 Process Extension Protocol

#### 版本目标

只修复 v1.2.2 和其他真实集成证明的协议瓶颈，不根据想象开放全部预留能力。

#### 候选能力与触发条件

| 候选能力 | 进入实现的证据要求 |
| --- | --- |
| 跨调用取消 | 长任务无法通过现有进程终止语义安全、及时取消 |
| Stream credit/backpressure | 有界 List 或文件投影无法满足内存/延迟目标，批量 API 也不足 |
| Opaque Handle | 连接、工作簿或游标需跨调用保持且不能安全序列化 |
| 进度事件 | 真实长任务需要机器可读进度，日志不足 |
| 协议并发 | 单请求串行成为已测瓶颈，扩展内部并发不能解决 |
| Capability 细化 | 现有声明导致真实场景过度授权 |

#### 协议设计要求

- Protocol 1 同步调用继续兼容；
- 新能力经协商启用，旧版本得到确定的不支持错误；
- request、handle、stream、cancel token 生命周期明确；
- credit、buffer、并发、消息大小和超时有硬上限；
- crash、cancel、提前 close 和协议错误均回收资源；
- transcript fixtures、跨版本矩阵和故障注入进入 CI；
- Checker 和 Contract Registry 可读取新增能力元数据。

#### 发布门槛

- 每项新增能力关联至少一个真实集成证据；
- 旧扩展无需重发即可运行；
- 新旧 Core/扩展组合有兼容矩阵；
- 取消、backpressure 和 handle 泄漏有压力测试；
- 仍优先使用进程隔离，不提前进入 Native ABI。

## 5. v1.3：真实用户与旗舰场景

### 版本目标

证明外部用户能够独立安装、理解、开发、部署和维护真实 HHY 项目。

### 推荐定位

> 面向 CI、数据收集和系统运维的安全确定性自动化语言。

### 旗舰项目

1. **FlowGuard**：CI 质量门禁入口，体现退出码、dry-run、安全和报告；
2. **多 API 数据采集器**：展示 Flow、HTTP、并发、重试和结构化数据；
3. **Crawler/SiteGraph Auditor**：高级案例，展示边界、SSRF 防护、恢复和渲染。

### 产品化工作

- `hhy init <template>` 创建官方模板；
- 模板包含配置、源码、测试、CI、README 和安全默认值；
- GitHub Actions 示例可直接复制；
- `hhy doctor` 检查 Core、扩展、lockfile 和宿主能力；
- 测量首次安装到成功运行的时间；
- 建立常见故障诊断文档；
- 官方模板锁定依赖并支持离线重建；
- 发布性能、内存和资源预算；
- 反馈模板记录任务、环境、阻塞点和放弃原因。

### 外部验证与门槛

- 至少 3–5 个非项目作者用户；
- 覆盖 CI、数据收集、系统运维中的至少两个场景；
- 用户无需作者远程操作即可部署；
- 至少一个项目持续运行一周或进入真实 CI；
- 功能诉求按频率、影响和替代方案归类；
- 形成公开案例、复现步骤和首次失败原因分布；
- 只有重复出现的真实阻塞进入后续路线。

### 进入 v1.4 前的语言语义基线

- 将规范中的冻结语义整理为独立 conformance suite；
- 测试覆盖 Value、Pipe、控制流、函数、Closure、Module、Stream、Error、取消、资源上限和退出码；
- 将实现细节测试与语言行为测试分开，后者不依赖 AST evaluator 内部结构；
- 为 diagnostics、stack trace、dry-run 和 profiler 建立稳定快照；
- 保存 v1.3 真实项目的 profile corpus，作为是否启动 Bytecode 的决策输入；
- 没有完成 conformance suite，不进入 v1.4 compiler/VM 开发。

## 6. v1.4：有条件的 AST → Bytecode VM 演进

### 版本定位

v1.4 是条件版本，不因 v1.3 完成而自动启动。它只解决已经被真实项目和 profiler 证明的 AST dispatch、节点遍历、环境查找或调用开销，不增加语言语法，不改变扩展分发路线，也不与 Native ABI 同时开发。

AST evaluator 在迁移期继续作为语义基准和回退引擎。Bytecode VM 必须复用现有 Parser、Checker、Resolver、Value、Stream、Error、Effect、Cancellation 和资源限制 contract，不能形成第二套语言行为。

### 启动条件

以下条件必须同时满足：

- v1.3 已取得至少 3–5 个外部真实项目的 profile；
- 至少两个 CPU-bound 工作负载持续显示 AST dispatch/节点遍历为主要热点；
- Resolver、slot、CallFrame、identifier cache、allocation 和算法优化已经实施；
- 工作负载瓶颈不是 HTTP、文件、扩展 IPC、序列化或错误的调用粒度；
- 预期收益足以覆盖 compiler、VM、debug info、stack trace 和双引擎测试的维护成本；
- 已冻结的 v1 语言语义可以保持不变。

未满足条件时跳过 v1.4，项目可直接继续维护 AST evaluator，或进入不涉及执行引擎的后续版本。

### 6.1 v1.4.0-alpha：Bytecode 设计与编译器骨架

#### 目标

建立可验证、可反汇编、暂不作为默认执行路径的 Bytecode IR 和 AST → Bytecode compiler。

#### 具体内容

| 模块 | 计划内容 |
| --- | --- |
| Chunk | 指令数组、常量池、源码位置表、函数元数据、最大 slot/stack 深度 |
| Opcode | 常量、slot/global/closure 访问、算术比较、跳转、调用、返回、集合、Pipe、异常、资源检查 |
| Compiler | 在 Parser、Checker、Resolver 后把 AST lowering 为 Bytecode，不重复解析或类型规则 |
| Control flow | if/else、短路逻辑、循环、break/continue、return、try/catch/throw 的跳转与 unwind |
| Function | 参数 slot、局部 slot、closure capture/upvalue、递归和嵌套函数 |
| Debug info | 每条指令映射原文件、行、列和 AST site，保留现有错误定位 |
| Tooling | `hhy compile`、`hhy bytecode` 或等价的内部 dump/verify 命令 |

#### 初始指令设计原则

- 采用小而明确的操作码集合，暂不做复杂超级指令；
- 操作数宽度、字节序、跳转范围和最大 chunk 大小明确；
- 常量池、函数和源码位置均有硬上限；
- 编译期间检查 stack effect 和跳转目标；
- Bytecode 只作为内存内部格式，alpha 阶段不承诺磁盘格式兼容；
- 不把 `HhyValue` 内部布局暴露为公共 ABI；
- 系统 I/O、Stream 和扩展调用继续经过现有 Runtime/Contract Registry。

#### Alpha 发布门槛

- literals、变量、表达式、分支、循环和函数可编译并反汇编；
- verifier 能拒绝非法 opcode、越界常量、错误跳转和不一致 stack effect；
- compiler 对源码错误继续使用现有 diagnostics；
- fuzz 可同时覆盖 Parser → Compiler → Verifier；
- 默认 `hhy run` 仍使用 AST evaluator。

### 6.2 v1.4.0-beta：Bytecode VM 执行核心

#### 目标

实现可选择运行完整语言核心的 VM，并与 AST evaluator 做逐用例语义对照。

#### VM 核心

- 明确定义 operand stack、CallFrame、instruction pointer 和 frame limit；
- 局部变量和参数走 slot 快路径；
- closure/upvalue 保持 escape-safe 生命周期；
- 函数调用、递归深度、return 和 tail position 行为与 AST 一致；
- try/catch/throw 保留 Error kind、code、stage、cause 和 stack；
- 每个循环/调用边界执行取消与资源预算检查；
- GC 根覆盖 operand stack、frames、constants、closures 和临时值；
- Stream 仍使用统一 `open/next/close`，不复制一套 Bytecode Stream；
- filesystem、process、HTTP、extension 和 effect 仍调用既有 Runtime 边界。

#### 双引擎执行方式

建议迁移期提供内部或实验开关：

```text
hhy run --engine ast script.hhy
hhy run --engine bytecode script.hhy
hhy check script.hhy
hhy bytecode script.hhy
```

该开关在 Bytecode 稳定前不承诺为长期公开 API，但 CI 必须能够分别执行两个引擎。

#### 语义对照测试

每个有效 fixture 同时对比：

- stdout、stderr 和 exit code；
- 返回值和值类别；
- Error kind/code/source/stage/cause/stack；
- side-effect plan 与 dry-run 输出；
- 资源限制、timeout、取消和提前 close；
- Map 迭代顺序、数字 equality/hash、Unicode 和整数溢出；
- closure、模块、Stream、parallel 和扩展调用；
- profiler 可识别 VM frame 和源码位置。

无效源码继续由共同 Parser/Checker 拒绝，不允许两个引擎产生不同语法或静态语义。

#### Beta 发布门槛

- 全部核心 valid/invalid/invalid-runtime fixtures 通过双引擎对照；
- Release、ASan/UBSan、GC stress 和 fuzz 在 Bytecode 路径通过；
- extension protocol transcript 与 AST 路径一致；
- 强制取消、OOM、递归上限和扩展崩溃不会泄漏 VM roots/resources；
- Bytecode 引擎仍不是默认执行路径。

### 6.3 v1.4.0-rc：性能、可观测性与默认切换评估

#### 目标

证明 Bytecode 在真实负载上有足够收益，并且诊断、Profiler、资源安全和跨平台行为没有退化。

#### 性能验收

- 对 v1.1.6 建立的 benchmark 同时运行 AST 和 Bytecode；
- 分开报告编译时间、执行时间、总墙钟、CPU、峰值 Heap 和二进制体积；
- 短脚本必须计入 compile overhead，不能只比较热循环；
- CPU-bound 旗舰负载应给出稳定、可复现的实质收益；
- I/O-bound 项目不得出现明显回退；
- benchmark 绑定 commit、编译器、平台、输入和重复次数。

#### 可观测性

- stack trace 显示 HHY 函数和源码位置，而不是裸 instruction offset；
- profiler 能汇总函数、源码和 opcode 热点；
- `hhy bytecode` 输出稳定的人类可读反汇编；
- crash/diagnostic 报告标记实际执行引擎；
- Debug build 可检查 stack、frame、upvalue 和 GC root invariants。

#### 默认切换门槛

只有同时满足以下条件，Bytecode 才可成为默认引擎：

- 全量双引擎语义测试零差异；
- 三个正式平台及 Windows smoke 均通过；
- sanitizer、GC stress、fuzz 和故障注入无新增阻断；
- CPU-bound 真实项目获得明确收益，短脚本无不可接受回退；
- diagnostics、stack trace、profile 和 dry-run 不低于 AST 体验；
- 至少一个 RC 周期未出现必须回退的正确性问题。

若门槛未满足，AST 继续作为默认引擎，Bytecode 保持实验状态或终止开发；不能为兑现版本号强行切换。

### 6.4 v1.4.x：迁移与兼容策略

- v1.4.0：满足门槛时默认启用 Bytecode，同时保留 `--engine ast` 回退；
- v1.4.1：修复真实负载差异，继续运行全部双引擎测试；
- v1.4.2：固化 compiler/VM 内部边界，持续运行 AST/Bytecode 对照；
- AST evaluator 在整个 v1.x 保留为语义 oracle、调试回退和差分测试引擎，不在 patch 版本删除；
- 将来若考虑移除 AST 执行入口，必须作为单独的重大兼容决策，不与普通稳定化版本捆绑；
- Bytecode 磁盘缓存另行评估，不能在内部格式尚未稳定时默认持久化；
- 若未来持久化 `.hhyc`，必须包含格式版本、源码/依赖指纹、平台无关性说明和安全 verifier；
- 不加载未经 verifier 验证的外部 Bytecode；
- Bytecode 版本与语言版本、扩展协议版本、包版本分别管理。

### v1.4 明确不做

- 不在同一版本开放 Native ABI；
- 不同时实现 JIT、LLVM 或本地编译；
- 不为了简化 VM 改变动态 Value、Pipe、Stream 或 Error 语义；
- 不让扩展直接提交未验证 Bytecode；
- 不承诺 alpha 阶段的 opcode 或二进制格式稳定；
- 不在缺少真实性能收益时移除 AST evaluator。

## 7. v2.0：有条件的生态开放与 ABI 决策

### 版本目标

判断是否需要 Embedding、FFI 或 Native ABI。v2.0 不是默认的 Native ABI 发布版；继续使用进程协议也是合法结论。

### 进入评估的硬条件

- 至少两个相互独立的真实集成；
- 已使用 v1.2.3 可提供的批量、流式、取消或 handle 能力；
- profiler 证明瓶颈来自进程边界，而非扩展实现、数据模型或调用粒度；
- 进程隔离收益无法抵消性能/能力损失；
- Runtime 对象布局、所有权和生命周期足够稳定。

### 必须比较的方案

| 方案 | 评估内容 |
| --- | --- |
| 继续使用 Process Protocol | 隔离、安全、部署简单性及性能是否足够 |
| 批量/共享文件优化 | 能否在不开放内存布局时解决传输问题 |
| Embedding API | 宿主控制 Runtime 是否更安全 |
| 受限 FFI | 只开放纯函数或稳定类型是否足够 |
| Native ABI | 崩溃面、GC、兼容、平台工具链和撤销成本 |

### 如果开放 Native ABI

- 只公开 opaque handle 和版本化函数表；
- 结构体含 `size` 和 `abi_version`；
- 新字段只追加，不公开 Runtime 内部结构；
- 字符串显式长度；参数明确 owned/borrowed；
- 扩展不得持有未经注册的 managed pointer；
- Stream、Error、Cancel、capability 继续统一；
- 安装时展示 Native 扩展的信任级别和崩溃风险；
- 随版本发布兼容矩阵、样例扩展和 sanitizer harness。

### 发布门槛

- 完成书面的进程协议边界和替代方案评估；
- 至少两个真实集成支持决策；
- 不开放时记录继续使用进程协议的理由；
- 开放时具备兼容策略、安全模型、工具链和长期维护承诺；
- 不把 Bytecode VM 与 ABI 决策捆绑重构。

## 8. 跨版本持续要求

| 方向 | 持续要求 |
| --- | --- |
| 安全 | 路径、网络、解包、签名、权限和资源上限默认安全 |
| 兼容 | 已发布语义、退出码、协议和 lock schema 有兼容策略 |
| 测试 | Release、Debug、sanitizer、GC stress、fuzz、跨平台、故障注入 |
| 性能 | 基准绑定 commit、环境和输入；回退必须解释或审批 |
| 文档 | README、规范、官网、限制、路线图与实现一致 |
| 证据 | 发布报告记录命令、commit、平台、产物和 digest |
| 体验 | 错误可定位、可修复；自动化输出机器可读 |

## 9. 近期明确不做

- 不增加第二套 Pipe、Stream、Error 或取消模型；
- 不在 v1.2 同时开放远程分发、Native ABI 和多套复杂扩展；
- 不公开 Runtime 内部 C 结构体；
- 不在没有真实需求时实现所有预留协议能力；
- 不把 JIT、本地编译或 Bytecode VM 自动绑定 v2.0；
- 不同时追求 Shell、ETL、爬虫、办公自动化和通用语言五种定位；
- 不以赶版本为理由跳过安全、跨平台和故障恢复验收。

## 10. 衡量指标

| 类别 | 指标 |
| --- | --- |
| 工程质量 | CI 成功率、flaky 次数、sanitizer/GC/fuzz 覆盖、benchmark 回退、发布证据完整率 |
| 开发体验 | 首次成功运行时间、诊断定位率、LSP 延迟/崩溃率、文档不一致次数 |
| 扩展工具链 | 安装成功率、验签覆盖率、lock 重建一致率、离线成功率、故障恢复率 |
| 协议质量 | 调用延迟、吞吐、取消延迟、资源泄漏、跨版本兼容率 |
| 用户验证 | 外部项目数、一周留存、首次失败原因、主要场景、真实放弃案例 |

## 11. v1.1.6 验证说明

初始路线评估期间执行：

```sh
make test
```

测试完成版本校验并进入 Runtime suite。受限宿主禁止 `/bin/ps`，相关验收明确跳过；随后本地 HTTP 测试服务器因禁止监听 `127.0.0.1` 失败：

```text
PermissionError: [Errno 1] Operation not permitted
```

该结果符合环境能力限制，而非 Runtime 功能回归。v1.1.6 已完成对应修复：测试启动时统一探测 process snapshot 和 socket bind；受限宿主输出稳定 `SKIP[HHY_CAP_*]` reason code，具备能力的 GitHub Actions runner 仍强制执行相关验收。修复后本地 Release/Debug suite 以及远端全部平台工作流均通过。

## 12. 最终优先级

```text
稳定性与测试治理
    > 结构化诊断与编辑器闭环
    > 官方扩展可信分发与可复现安装
    > 复杂扩展对协议的真实验证
    > 外部用户与旗舰项目
    > 证据驱动的 AST → Bytecode VM
    > 有条件的 ABI 决策
```

HHY 当前已经足够“能做事”。接下来要证明的是：用户能够可信地安装能力、低成本地开发和排错，并愿意持续使用 HHY 完成真实任务。
