# 香港电影公司：HHY 维基百科并发研究

这个项目使用 HHY 调用维基百科官方 MediaWiki API，以“香港電影公司”为关键词搜索候选页面，并发抓取每个页面的简介与正式链接，筛选公司条目后原子写入 CSV 和 JSON 报告。

## 使用的 HHY 特性

- `http.get`、query 参数和自定义 `User-Agent`；
- `timeout + retry + attempt` 网络容错；
- `parallel(3)` 有界并发和保序输出；
- `Stream + group_by + where + sort_by` 去重和数据清洗；
- `parse_json + encode_json + encode_csv` 结构化数据转换；
- `save_text/save_lines({ atomic: true })` 原子落盘。

## 抓取真实维基百科数据

在仓库根目录执行：

```sh
./build/hhy run \
  practical-projects/hong-kong-film-companies/crawl.hhy \
  practical-projects/hong-kong-film-companies/config/wikipedia.json \
  practical-projects/hong-kong-film-companies/output/report.json \
  practical-projects/hong-kong-film-companies/output/hong-kong-film-companies.csv
```

输出字段包括公司名称、维基百科 page ID、页面 URL、词数、页面更新时间和首段简介。数据来自维基百科动态搜索结果，不应视为完整公司名录；再次运行时结果可能变化。

CSV 保留每条数据的维基百科 URL，便于核验与署名。维基百科文本通常依 CC BY-SA 许可提供；公开再发布包含简介的 CSV 前，应遵循对应页面标注的许可与署名要求。

`config/wikipedia.json` 中的 `search_url` 使用已经百分号编码的中文关键词。这是为了兼容 HHY 1.1.0 当前对非 ASCII query Map 值的编码限制；详情请求的 ASCII 参数仍由 HHY query Map 安全构造。

## 确定性自测

```sh
sh practical-projects/hong-kong-film-companies/self-test.sh
```

自测使用本地模拟 MediaWiki API，验证 5 个候选页面经过并发抓取和语义过滤后准确输出 3 家公司，并检查 CSV 字段、排序、URL 和 JSON 统计。
