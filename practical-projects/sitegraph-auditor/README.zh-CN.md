# SiteGraph Auditor

SiteGraph Auditor 是一个基于 HHY 1.1.5 可恢复 Spider 的站点结构与内容质量门禁。它递归抓取授权范围内的文档页面，生成页面 inventory、规范化链接图、失败归档和审计报告。

## 挑战点

- 相对 URL、点路径、fragment 和默认端口通过 `url_resolve` 统一规范化。
- HTML 扩展从每页发现链接；Frontier 按深度推进并有界并发。
- 域名、路径、深度、页面数、Frontier 和单页链接数量都有硬限制。
- 规范 URL 指纹在入队前去重，链接图还统计同一来源的重复边。
- Crawler 默认设置 `allow_private_networks: false`，在 DNS 解析及重定向后的实际连接地址上阻止 SSRF。
- 内容门禁检查 title、description、canonical 与抓取失败，并使用稳定退出码。

## 输出

- `inventory.json`：页面标题、描述、canonical、主标题与来源 URL。
- `graph.json`：规范化链接边、是否允许及拒绝原因。
- `report.json`：抓取、去重、越界、限制、错误与 warning 汇总。
- `failures.json`：逐页抓取失败。

## 确定性自测

正常运行默认复用 HHY 统一的用户级扩展目录 `~/.hhy/extensions`，不会在项目内保存插件副本。CI/自测通过临时 `HHY_EXTENSION_HOME` 隔离安装，测试退出后自动清理。

```sh
make
./practical-projects/sitegraph-auditor/self-test.sh
```

健康 fixture 有四层页面、相对链接、点路径、fragment 重复和跨域引用，必须通过。风险 fixture 同时包含缺失 description/canonical、404 链接和越界路径，必须稳定失败。最后一项负向测试证明安全模式会拒绝 loopback。
