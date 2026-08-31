# 15. 实战项目：多 API 数据采集器

并发采集 OpenAlex、Crossref 和 GitHub 分页数据，统一字段、记录失败并增量汇总为 CSV。

## 15.1 一条可靠的数据采集 Flow

这个项目使用 HHY v1.2.0 同时采集 OpenAlex 学术成果、Crossref 论文元数据和 GitHub 仓库。每个来源请求 2 页，HTTP 下载由 parallel(3) 有界并发完成，异构 JSON 随后统一成 source、external_id、title、url、record_type、metric_name 和 metric_value。


| 能力 | 实现 |
| --- | --- |
| 分页与并发 | 6 个任务 + parallel(3) |
| 网络容错 | 250ms 限速 + 10s timeout + 2 次 retry + attempt |
| 数据治理 | 字段统一 + source/external_id 去重 + 稳定排序 |
| 增量保存 | 读取已有 CSV，新记录覆盖同键旧记录，atomic save |
| 故障审计 | 失败来源、页码和错误写入 failures.json |


[在 GitHub 查看完整源码 ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/multi-api-data-collector)

包含 HHY 入口、分页任务、三源适配器、增量合并、配置和确定性自测。


## 15.2 项目结构

![HHY 多 API 数据采集器目录结构](https://hhylang.dev/multi-api-data-collector-tree.png)

_collector.hhy 负责编排，jobs.hhy 生成分页任务，sources.hhy 下载并标准化三种 API，merge.hhy 完成去重与增量合并。_


| 文件 | 职责 |
| --- | --- |
| collector.hhy | 组合采集、统计和原子输出 |
| lib/jobs.hhy | 生成 OpenAlex、Crossref、GitHub 分页任务 |
| lib/sources.hhy | 限速、超时、重试、并发下载和字段统一 |
| lib/merge.hhy | 读取旧 CSV、去重、排序和增量覆盖 |


## 15.3 运行与增量结果

```sh
./build/hhy run practical-projects/multi-api-data-collector/collector.hhy \
  practical-projects/multi-api-data-collector/config/public-apis.json \
  practical-projects/multi-api-data-collector/output/records.csv \
  practical-projects/multi-api-data-collector/output/report.json \
  practical-projects/multi-api-data-collector/output/failures.json
```


![HHY 多 API 数据采集器自测终端结果](https://hhylang.dev/multi-api-data-collector-run.png)

_确定性端到端验证：6 页、12 条输入、9 条唯一记录；第二次运行读取 9 条旧记录，增量合并后仍为 9 条。_


{% hint style="info" %}
公开 API 数据和额度会变化。正式输出保留来源 URL；本地 fixture 只用于回归测试并在退出后删除，不会写入正式 output。
{% endhint %}


## 15.4 验证

```sh
sh practical-projects/multi-api-data-collector/self-test.sh
```


测试连续运行两次，验证分页、并发、字段统一、跨页去重、稳定排序、失败列表和增量覆盖。并发用于等待 HTTP，而不是把数值计算伪装成 HHY 的优势。
