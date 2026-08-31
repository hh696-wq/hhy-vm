# 12. 实战项目：DataFlow ETL

从 CSV、JSON 目录和 HTTP API 同步数据，完成清洗、过滤、并发补全、分组汇总及 JSON/CSV 双输出。

## 12.1 完整的数据同步管道

DataFlow ETL 是完全由 HHY v1.2.0 运行并通过端到端自测的数据同步应用。它读取客户 CSV 和事件 JSON 目录，规范化姓名与邮箱，过滤停用和低消费客户，并发请求本地画像 API，按部门 group_by 汇总，最后原子写入 JSON 报告和 CSV 明细。


| 阶段 | 实现 |
| --- | --- |
| 采集 | read_lines + parse_csv；files + parse_json |
| 清洗 | trim、lower、to_int 与结构化 Map |
| 补全 | parallel(4) + http.get + timeout + retry |
| 汇总 | where、sort_by、group_by、sum |
| 输出 | encode_json/save_text 与 encode_csv/save_lines |


[在 GitHub 查看 DataFlow ETL 完整源码 ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/dataflow-etl)

包含 HHY 模块、CSV/JSON 测试数据、画像 API、报告断言和一键自测。


## 12.2 真实目录与数据流

![DataFlow ETL 项目目录树](https://hhylang.dev/dataflow-etl-tree.png)

_真实项目目录：入口、四个 HHY 模块、CSV/JSON fixtures、配置和测试工具。_


```text
customers.csv + events/*.json + HTTP profiles
                    ↓
            parse / trim / lower
                    ↓
          active + minimum spend filter
                    ↓
          parallel HTTP enrichment
                    ↓
          group_by department + sum
                    ↓
              report.json + customers.csv
```


## 12.3 实际自测结果

```sh
cd hhy-vm
sh practical-projects/dataflow-etl/self-test.sh
```


![DataFlow ETL 实际端到端自测结果](https://hhylang.dev/dataflow-etl-self-test.png)

_真实运行：3 条合格客户、2 个事件文件、2 个部门汇总，HTTP 补全及 JSON/CSV 断言全部通过。_


{% hint style="info" %}
测试服务只监听 127.0.0.1:18992。测试会验证客户过滤和排序、邮箱清洗、远程 region/tier 字段、部门消费汇总以及两种输出格式。
{% endhint %}


## 12.4 运行自己的同步任务

复制 config/test.json，替换项目名、API 地址和最低消费阈值，再准备 customers.csv 与 events/*.json。HTTP 单项失败会变成结构化 error，报告 ok=false 并返回退出码 1。


```sh
hhy run practical-projects/dataflow-etl/etl.hhy \
  ./input \
  ./config.json \
  ./output/report.json \
  ./output/customers.csv
```
