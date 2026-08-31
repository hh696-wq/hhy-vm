# 22. HTML 扩展与抓取框架

用官方 HTML 扩展和安全 Frontier 完成 URL 规范化、链接发现、递归去重及有界静态抓取。

## 22.1 HTML 扩展是什么

官方 html 0.1.0 是一个无网络、无文件副作用的进程扩展。它用 Lexbor 解析不可信 HTML、执行 CSS Selector，并把文本、属性或重复记录转换成 HHY 可直接处理的 String、Bool、List 和 Map。


{% hint style="info" %}
HTML 扩展只负责解析和抽取，不下载 URL、不调度页面，也不返回 DOM handle。HTTP、TLS、超时、重试和安全策略由 HHY Runtime 与上层爬虫框架负责。
{% endhint %}


## 22.2 构建与安装 HTML 扩展

```sh
brew install jansson lexbor
make -C extensions/html
./build/hhy install ./extensions/html
./build/hhy list
```


| 完整签名 | 用途 |
| --- | --- |
| html.text(String html, String selector, Map?) -> String? | 读取首个节点的规范化文本 |
| html.text_all(String html, String selector, Map?) -> List<String> | 读取全部匹配节点的文本 |
| html.attr(String html, String selector, String name, Map?) -> String? | 读取首个节点的属性 |
| html.attr_all(String html, String selector, String name, Map?) -> List<String> | 读取全部匹配节点的属性 |
| html.exists(String html, String selector) -> Bool | 判断 Selector 是否命中 |
| html.extract(String html, String selector, Map schema, Map?) -> List<Map> | 一次解析文档，并按 schema 投影重复记录 |


| 选项 | 适用方法 | 行为 |
| --- | --- | --- |
| trim: Bool | text、text_all、attr、attr_all | 是否清理首尾空白，默认开启 |
| max_results: Int | text_all、attr_all、extract | 默认 1000，硬上限 10000 |


扩展把输入限制在 768 KiB，以便协议消息始终有界。extract 的 schema 字段使用 { selector, value: "text" } 或 { selector, value: "attr", name }；空 selector 表示读取当前 root。它不返回 DOM handle：Protocol 1 只运输 JSON 可表达的值，因此 html.extract 在扩展内一次完成解析和字段投影。


## 22.3 从 HTML 扩展到安全爬虫框架

静态 Spider 把多个职责组合成闭环：Runtime 的 url_resolve 规范化并解析相对 URL；HTML 扩展发现链接；按深度推进的 Frontier 在入队前检查域名、路径、页面数、队列大小和请求指纹；HTTP 网络层在实际连接地址上阻止 SSRF。


| 能力 | 当前实现程度 |
| --- | --- |
| url_resolve(url, base?) | 返回 url、scheme、host、port、path、query 与稳定 fingerprint |
| 链接发现 | follow_selector 提取 href，逐层加入 Frontier |
| Frontier 与去重 | 按深度、有界并发；规范 URL 指纹在入队前去重 |
| 边界限制 | allowed_domains、path prefixes、max_depth/pages/frontier/links |
| SSRF 防护 | 在每次实际 socket 连接时阻止 loopback、私网及 link-local，覆盖重定向 |


## 22.4 项目一：my-crawler 基础闭环

```sh
make
./practical-projects/my-crawler/init.sh
./practical-projects/my-crawler/self-test.sh
./practical-projects/my-crawler/run.sh
```


my-crawler 是最小可运行 Spider：读取 JSON 配置，递归发现链接，按边界抓取并输出 records、report 和 failures。init.sh 默认复用 ~/.hhy/extensions；CI 与自测使用临时 HHY_EXTENSION_HOME。


**config/hhylang.json**

```text
{
  "seeds": ["https://hhylang.dev/zh/learn/cli-reference"],
  "allowed_domains": ["hhylang.dev"],
  "allowed_path_prefixes": ["/zh/learn/"],
  "follow_selector": "main article a[href]",
  "parallelism": 2,
  "max_depth": 2,
  "max_pages": 50,
  "max_frontier": 100,
  "max_links_per_page": 200,
  "allow_private_networks": false,
  "root_selector": "main article h2",
  "max_results": 100,
  "schema": { "title": { "selector": "", "value": "text" } }
}
```


```console
$ ./practical-projects/my-crawler/self-test.sh
HHY Collector Framework Crawler Fixture
Pages 3 / 3 Records 3 Failures 0
HHY Collector Framework self-test passed
```


[查看 my-crawler 完整源码](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/my-crawler)

适合先理解配置、递归抓取、结构化抽取、失败归档和确定性自测。


## 22.5 项目二：SiteGraph Auditor 挑战项目

SiteGraph Auditor 在基础 Spider 上增加页面 inventory、规范化链接图、metadata 审计和 CI 质量门禁，同时运行健康站点与风险站点双场景。


| 输出 | 内容 |
| --- | --- |
| inventory.json | 页面 metadata、主标题和来源 URL |
| graph.json | 规范化链接边、fingerprint、允许状态与拒绝原因 |
| report.json | 页面、边、重复、限制、错误、warning 与 findings |
| failures.json | 失败 URL、深度和稳定错误 |


[进入 SiteGraph Auditor 挑战项目](/zh/learn/sitegraph-auditor-project)

继续完成站点图谱、内容质量审计、SSRF 负例和 CI 门禁。


## 22.6 适用范围与明确边界

当前支持内存或原子 checkpoint Frontier、严格配置匹配的断点恢复，以及 send_to 流式响应文件。可选 Playwright Renderer 能执行 JavaScript；它与无副作用的 Lexbor HTML 扩展隔离，并对主文档、重定向和子资源执行域名与 DNS 私网检查。


{% hint style="info" %}
不要用它绕过 robots.txt、登录、验证码或反爬策略。只采集你有权访问的站点，并保持可识别 User-Agent、保守并发和明确页面上限。
{% endhint %}
