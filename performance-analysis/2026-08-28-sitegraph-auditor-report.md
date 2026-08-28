# SiteGraph Auditor 实战项目实现与验收报告

日期：2026-08-28
目标：用 HHY HTML 扩展和安全爬虫基础设施实现一个可重复验收的站点图谱与内容质量审计项目。

## 1. 交付结果

新增 `practical-projects/sitegraph-auditor/`。项目从一个种子 URL 递归抓取静态站点，生成页面清单、规范化链接图、重复链接统计、拒绝链接统计和内容质量报告，并以退出码充当 CI 质量门禁。

它不是单页抓取示例，而是完整使用以下能力的最小生产闭环：

1. URL 规范化：相对链接、点路径和片段统一解析为规范 URL。
2. 链接发现：从每个成功页面继续发现下一层链接。
3. Frontier：按待抓取队列持续推进，不依赖预先提供 URL 列表。
4. 域名、路径、深度和页面限制：控制抓取边界与资源预算。
5. 指纹去重：同一规范 URL 只进入 Frontier 一次，同时统计重复边。
6. SSRF 防护：默认拒绝环回、私网及其他非公网目标；测试夹具仅通过显式测试开关访问本地服务。

## 2. 工程设计

- `sitegraph.hhy`：编排抓取、图谱、审计报告与退出码。
- `lib/graph.hhy`：对发现链接再次执行 `url_resolve`，应用域名和路径边界，构建确定性的规范化边。
- `lib/report.hhy`：检查标题、描述、canonical 和抓取失败，输出稳定的错误/警告计数。
- `test-server.py`：同时提供健康四层文档站与故障站，覆盖相对路径、点路径、片段重复、外域链接、越界路径和 404。
- `self-test.sh` / `test-report.py`：精确断言页面数、边数、重复数、拒绝数、发现项和质量门禁退出码，并额外验证 SSRF 负例。
- `.github/workflows/ci.yml`：Linux 与 macOS CI 都执行原安全爬虫和 SiteGraph Auditor 的端到端验收。

## 3. 本地举证

### SiteGraph Auditor 自测

```text
SiteGraph Auditor healthy
Pages 4 / 4 Edges 5
Duplicates 3 Rejected 1 Findings 0
SiteGraph Auditor risky
Pages 1 / 2 Edges 2
Duplicates 0 Rejected 1 Findings 3
SiteGraph Auditor self-test passed
```

健康站点覆盖四层递归、规范化与去重；风险站点覆盖缺少 description、缺少 canonical、404 和路径越界，并按预期返回非零质量门禁状态。环回地址在未开启测试例外时被 SSRF 策略拒绝。

### 全仓回归

执行 `make test`，最终结果：

```text
all lexer, parser and runtime tests passed
```

首次在受限沙箱内运行时，本地 HTTP 测试服务因无权绑定 `127.0.0.1` 而未启动；允许测试绑定临时本地端口后完整通过。该现象属于执行环境限制，不是产品代码失败。

### Website 验收

执行 `npm run check`，通过 HHY 示例检查、ESLint、TypeScript 和 Next.js 生产构建，共生成 56 个页面。

在 `http://localhost:9100/zh/learn/sitegraph-auditor-project` 做真实浏览器验收：六项新能力、健康/风险输出均可见，无 Next.js 错误遮罩，无控制台错误。

## 4. 能力边界

该项目已经具备安全递归静态爬虫的工程闭环，适合文档站、博客、知识库和服务端渲染站点的链接图谱与元数据质量审计。当前不包含 JavaScript 页面渲染、robots.txt/sitemap 调度、跨进程持久化 Frontier、分布式抓取和按站点自适应限速；这些应作为后续独立能力演进，而不应削弱当前默认安全边界。
