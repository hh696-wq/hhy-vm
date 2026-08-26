# 多 API 数据采集器

这个实战项目用一条 HHY Flow 并发采集 OpenAlex 学术成果、Crossref 论文元数据和 GitHub 仓库数据。三个 API 的异构 JSON 被统一成相同字段，去重、稳定排序并增量合并到 CSV。

## 覆盖的生产能力

- 多来源分页请求与 `parallel(3)` 有界并发；
- 每个请求前等待 250ms，避免突发流量；
- `timeout(10s) + retry(2) + attempt` 容错；
- 失败页面独立写入 `failures.json`；
- JSON 字段统一为来源、外部 ID、标题、URL、类型和指标；
- 按 `source + external_id` 去重并稳定排序；
- 读取已有 CSV，新数据覆盖同键旧记录后原子保存；
- JSON 运行报告与 CSV 汇总同时落盘。

## 真实采集

在仓库根目录执行：

```sh
./build/hhy run \
  practical-projects/multi-api-data-collector/collector.hhy \
  practical-projects/multi-api-data-collector/config/public-apis.json \
  practical-projects/multi-api-data-collector/output/records.csv \
  practical-projects/multi-api-data-collector/output/report.json \
  practical-projects/multi-api-data-collector/output/failures.json
```

默认主题是 `artificial intelligence`，每个来源抓取 2 页、每页 5 条。公开 API 数据会变化；GitHub 未认证搜索的额度较低，生产使用时应进一步降低频率或通过安全的进程扩展注入认证请求。

数据来源：[OpenAlex API](https://docs.openalex.org/)、[Crossref REST API](https://www.crossref.org/documentation/retrieve-metadata/rest-api/)、[GitHub REST API](https://docs.github.com/en/rest/search/search)。CSV 保留每条数据的原始 URL，便于追溯。

## 确定性自测

```sh
sh practical-projects/multi-api-data-collector/self-test.sh
```

自测服务器只在临时目录和本机随机端口运行，退出后自动删除。脚本连续采集两次，验证 6 个分页任务、跨页去重、稳定排序、增量覆盖以及 9 条最终记录，不会向正式 `output/` 写入模拟数据。
