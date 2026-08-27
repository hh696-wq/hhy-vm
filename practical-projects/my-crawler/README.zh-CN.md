# HHY Collector Framework

这是一个 Flow-first、配置可审计、资源有边界的静态文档采集框架。它用 HHY 负责 HTTP、重试、有界并发、错误归档和原子输出，用官方 `html` 进程扩展负责 CSS Selector 抽取。

## 初始化并运行真实任务

在仓库根目录先构建 HHY，然后运行：

```sh
make
./practical-projects/my-crawler/init.sh
./practical-projects/my-crawler/run.sh
```

默认任务会抓取 `https://hhylang.dev/zh/learn/cli-reference`，抽取页面中的二级标题，并写入 `output/records.json`、`output/report.json` 与 `output/failures.json`。

`init.sh` 把扩展安装到项目自己的 `.hhy-extensions`，不会污染用户级扩展目录；重复运行是安全的。

## 配置一个 Spider

编辑 `config/hhylang.json`：`seeds` 是入口页，`parallelism` 是有界并发，`root_selector` 定义每条记录的根节点，`schema` 定义字段。字段支持 `text`、`html` 和 `attr`，也支持 `all`；空 selector 表示读取记录根节点本身。

当前边界很明确：响应体完整缓冲，适合 API 与静态 HTML；不执行 JavaScript，不规避 robots.txt、登录验证或反爬策略。请只抓取你有权访问的站点，并使用可识别的 User-Agent 与保守并发。

## 验证

```sh
./practical-projects/my-crawler/self-test.sh
```

自测使用本机 fixture server，完全确定性，不依赖公网。
