# 12. Project: DataFlow ETL

Synchronize CSV, JSON-directory, and HTTP API data through cleaning, filtering, concurrent enrichment, grouping, and JSON/CSV outputs.

## 12.1 A complete synchronization pipeline

DataFlow ETL runs entirely on HHY v1.2.0 and is verified end to end. It reads customer CSV and an event JSON directory, normalizes names and email addresses, filters inactive and low-spend customers, calls a profile API concurrently, aggregates departments with group_by, and atomically writes JSON and CSV outputs.


| Stage | Implementation |
| --- | --- |
| Ingest | read_lines + parse_csv; files + parse_json |
| Clean | trim, lower, to_int, and structured Maps |
| Enrich | parallel(4) + http.get + timeout + retry |
| Aggregate | where, sort_by, group_by, and sum |
| Output | encode_json/save_text and encode_csv/save_lines |


[View the complete DataFlow ETL source on GitHub ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/dataflow-etl)

Includes HHY modules, CSV/JSON fixtures, profile API, report assertions, and one-command self-test.


## 12.2 Real layout and data flow

![DataFlow ETL project tree](https://hhylang.dev/dataflow-etl-tree.png)

_The real project layout: entry point, four HHY modules, CSV/JSON fixtures, configuration, and test utilities._


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


## 12.3 Actual self-test result

```sh
cd hhy-vm
sh practical-projects/dataflow-etl/self-test.sh
```


![Actual DataFlow ETL end-to-end result](https://hhylang.dev/dataflow-etl-self-test.png)

_Actual run: three qualified customers, two event files, two department summaries, and all HTTP enrichment plus JSON/CSV assertions passed._


{% hint style="info" %}
The test API binds only to 127.0.0.1:18992. Assertions cover filtering and ordering, email cleanup, remote region/tier fields, department spend totals, and both output formats.
{% endhint %}


## 12.4 Run your own synchronization

Copy config/test.json, replace the project name, API base, and minimum-spend threshold, then provide customers.csv and events/*.json. An individual HTTP failure becomes a structured error, sets report ok=false, and returns exit code 1.


```sh
hhy run practical-projects/dataflow-etl/etl.hhy \
  ./input \
  ./config.json \
  ./output/report.json \
  ./output/customers.csv
```
