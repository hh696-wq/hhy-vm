# HHY Collector Framework

这是一个 Flow-first、配置可审计、资源有边界的静态 Spider。它用 HHY 负责 URL 规范化、Frontier、HTTP、重试、有界并发、错误归档和原子输出，用官方 `html` 进程扩展负责链接发现与 CSS Selector 抽取。

## 初始化并运行真实任务

在仓库根目录先构建 HHY，然后运行：

```sh
make
./practical-projects/my-crawler/init.sh
./practical-projects/my-crawler/run.sh
```

默认任务会抓取 `https://hhylang.dev/zh/learn/cli-reference`，抽取页面中的二级标题，并写入 `output/records.json`、`output/report.json` 与 `output/failures.json`。

`init.sh` 默认复用 HHY 统一的用户级扩展目录（`~/.hhy/extensions`）；若 CI 或测试设置 `HHY_EXTENSION_HOME`，则使用指定的隔离目录。重复运行是安全的。

## 配置一个 Spider

编辑 `config/hhylang.json`：`seeds` 是入口页，`follow_selector` 发现下一层链接，`allowed_domains` 与 `allowed_path_prefixes` 限定抓取边界，`max_depth`、`max_pages`、`max_frontier` 设置硬上限。每层 Frontier 使用 `parallelism` 有界并发处理。

`url_resolve` 使用 libcurl URL API 合并相对 URL、消解 `.`/`..`、移除 fragment、统一 host 大小写和默认端口，并生成稳定请求指纹。已访问指纹在进入 Frontier 前去重。

Crawler 发出的请求默认设置 `allow_private_networks: false`。Runtime 在 libcurl 建立每一次实际连接时检查解析后的 IPv4/IPv6 地址，因此初始 URL和重定向目标都不能连接 loopback、私网或 link-local 地址。只有本机 fixture 测试显式开启该选项。

设置 `response_directory` 后，Runtime 使用 `send_to` 将响应流式写入原子文件，Crawler 再按需读取并交给 HTML 扩展，避免 HTTP 传输阶段持有第二份完整 BytesBuffer。设置 `checkpoint_path` 后，每个 Frontier 批次都会原子保存队列、指纹、页面和统计；`resume: true` 仅在版本与安全配置摘要完全匹配时恢复。

需要执行页面 JavaScript 时，可安装 `renderer/` 中锁定版本的 Playwright，并设置 `javascript_rendering: true`。Renderer 独立于无副作用的 Lexbor HTML 扩展；主文档、重定向与子资源都必须通过 allowed_domains 和 DNS 私网检查。不规避 robots.txt、登录验证、验证码或反爬策略。

## 验证

```sh
./practical-projects/my-crawler/self-test.sh
```

自测使用本机 fixture：覆盖三层 Frontier、流式响应文件、原子 checkpoint、断点恢复、动态 JavaScript 内容抽取及 SSRF 默认拒绝。测试完全确定性，不依赖公网。
