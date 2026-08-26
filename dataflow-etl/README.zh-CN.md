# DataFlow ETL

DataFlow ETL 是使用 HHY v1.0 编写的完整数据同步项目，真实串联 CSV、JSON 文件目录和 HTTP API：读取客户 CSV 与事件 JSON，清洗字段、过滤有效数据，并发补充远程画像，按部门分组汇总，最后原子写入 JSON 报告和 CSV 明细。

## 一键自测

```sh
cd /path/to/hhy-vm
sh dataflow-etl/self-test.sh
```

测试服务只监听 `127.0.0.1:18992`。断言覆盖清洗后的邮箱、过滤和排序结果、HTTP 补充字段、事件文件数量、部门聚合值，以及 JSON/CSV 两种输出。

## 数据流

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
