# 16. 实战项目：SiteGraph Auditor

递归建立文档站页面清单与规范化链接图，用 metadata、失败和安全边界实施质量门禁。

## 16.1 一个真正使用安全 Spider 的质量门禁

SiteGraph Auditor 基于 HHY v1.2.0 和 my-crawler 的安全递归引擎。它从 seed 开始逐层发现页面，同时输出 inventory、规范链接图、report 和 failures；报告以稳定退出码阻止缺失 metadata 或存在抓取失败的站点通过。


| 新能力 | 项目中的实际用途 |
| --- | --- |
| URL 规范化 | 相对链接、点路径、fragment、host 和默认端口统一成稳定 URL |
| 链接发现 | main a[href] 持续补充下一层 Frontier |
| Frontier | 按深度批次并发，保留 page/depth/source 上下文 |
| 硬边界 | domain/path/depth/pages/frontier/links 六类限制 |
| 指纹去重 | 进入 Frontier 前去重，链接图另统计同源重复边 |
| SSRF | 正式配置禁止私网，在实际 socket 地址上覆盖 DNS 与重定向 |


[查看 SiteGraph Auditor 完整源码 ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/sitegraph-auditor)

包含三个 HHY 模块、四层健康站点、风险站点、报告断言和 SSRF 负向测试。


## 16.2 健康与风险双场景

```sh
make
./practical-projects/sitegraph-auditor/self-test.sh
```


```console
$ ./practical-projects/sitegraph-auditor/self-test.sh
SiteGraph Auditor healthy
Pages 4 / 4 Edges 5
Duplicates 3 Rejected 1 Findings 0
SiteGraph Auditor risky
Pages 1 / 2 Edges 2
Duplicates 0 Rejected 1 Findings 3
SiteGraph Auditor self-test passed
```


健康站点包含四层页面、相对 URL、点路径、fragment 重复和跨域引用，必须通过。风险站点包含缺失 description/canonical、404 和越界路径，必须返回失败；最后再证明安全模式会拒绝 loopback。


## 16.3 输出与当前边界

| 输出 | 内容 |
| --- | --- |
| inventory.json | title、description、canonical、heading、source_url |
| graph.json | source、原始 href、规范目标、fingerprint、允许状态和拒绝原因 |
| report.json | 页面、边、重复、拒绝、限制、错误、warning 和 findings |
| failures.json | URL、深度和稳定错误 |


{% hint style="info" %}
默认模式仍是静态站点审计，不绕过认证、验证码、robots.txt 或反爬策略。v1.1.5 可选择原子 checkpoint Frontier 与严格断点恢复，也可通过隔离的 Playwright Renderer 执行 JavaScript。
{% endhint %}
