# 14. 实战项目：香港电影公司

使用 MediaWiki API 并发抓取“香港電影公司”候选页面，清洗、筛选并汇总为 CSV 和 JSON 报告。

## 14.1 用 HHY 完成真实网络数据研究

这个项目使用 HHY v1.2.0 调用维基百科官方 MediaWiki API。程序先按“香港電影公司”搜索 10 个候选页面，再用 parallel(3) 并发抓取 page ID、正式 URL、更新时间与限定长度的简介；随后筛选同时包含香港、电影和公司语义的条目，按标题排序并原子写入 CSV 与 JSON。


| 阶段 | HHY 实现 |
| --- | --- |
| 搜索 | http.get + timeout + retry + parse_json |
| 去重 | group_by(pageid) + Map |
| 详情 | parallel(3) + attempt，单页失败不终止其他任务 |
| 筛选 | Stream + where + contains + sort_by |
| 输出 | encode_csv/encode_json + atomic save |


[在 GitHub 查看完整源码 ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/hong-kong-film-companies)

包含 HHY 抓取程序、配置、模块、本地 MediaWiki fixture、CSV/JSON 断言和中英文说明。


## 14.2 项目结构

![香港电影公司 HHY 维基百科抓取项目目录树](https://hhylang.dev/hong-kong-film-companies-tree.png)

_真实目录：crawl.hhy 负责编排，api.hhy 负责搜索和并发详情请求，transform.hhy 负责去重、筛选、排序与报表结构。_


| 文件 | 职责 |
| --- | --- |
| crawl.hhy | 读取配置、组合数据流并原子输出 CSV/JSON |
| lib/api.hhy | MediaWiki 搜索、HTTP 策略和 bounded parallel |
| lib/transform.hhy | 语义过滤、稳定排序和输出字段 |
| self-test.sh | 启动本地 fixture 并验证 5→3 的确定性结果 |


## 14.3 真实维基百科运行结果

2026-08-26 的实际运行抓取 10 个候选页面，10 个详情请求全部成功，筛出 7 个公司相关结果，包括中國星集團、國泰機構、天下一電影、東方電影（香港）、邵氏兄弟、電影工作室和香港電影公司列表。


```sh
./build/hhy run --limit max_runtime=2min --limit max_parallelism=8 \
  practical-projects/hong-kong-film-companies/crawl.hhy \
  practical-projects/hong-kong-film-companies/config/wikipedia.json \
  practical-projects/hong-kong-film-companies/output/wikipedia-report.json \
  practical-projects/hong-kong-film-companies/output/wikipedia-hong-kong-film-companies.csv
```


![HHY 并发抓取香港电影公司维基百科数据的真实终端结果](https://hhylang.dev/hong-kong-film-companies-run.png)

_真实联网运行：Candidates 10、Fetched 10、Companies 7，并成功写出 CSV 和 JSON 报告。_


{% hint style="info" %}
维基百科搜索结果和页面内容会变化，因此这是可复现的动态研究样本，不是完整公司注册名录。项目的 self-test 使用本地模拟 API，避免把外部网络变化当成代码回归。
{% endhint %}


## 14.4 并发、边界与可重复验证

默认并发度为 3，每页简介限制为 600 字符，避免不受控响应占用 worker 通道。网络请求具有 10 秒 timeout 和两次 retry；每个详情任务由 attempt 隔离。中文搜索词在配置中的 search_url 已百分号编码，以兼容 HHY 1.1.1 当前非 ASCII query Map 编码限制。


```sh
sh practical-projects/hong-kong-film-companies/self-test.sh
```


[阅读中文运行说明 ↗](https://github.com/hh696-wq/hhy-vm/blob/main/practical-projects/hong-kong-film-companies/README.zh-CN.md)

查看输出字段、真实抓取命令、配置限制与确定性测试设计。
