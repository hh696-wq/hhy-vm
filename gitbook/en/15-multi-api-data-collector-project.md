# 15. Project: Multi-API Data Collector

Collect paginated OpenAlex, Crossref, and GitHub data concurrently, normalize it, record failures, and incrementally merge CSV.

## 15.1 One reliable collection flow

Built with HHY v1.2.0, this project collects two pages each from OpenAlex, Crossref, and GitHub. parallel(3) bounds concurrent HTTP downloads before heterogeneous JSON is normalized into one seven-column schema.


| Capability | Implementation |
| --- | --- |
| Pagination and concurrency | Six jobs with parallel(3) |
| Network resilience | 250ms pacing, 10s timeout, two retries, and attempt |
| Data governance | Normalization, composite-key deduplication, stable sorting |
| Incremental output | Read existing CSV, replace matching keys, atomic save |
| Failure audit | Source, page, and error in failures.json |


[View the complete source on GitHub ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/multi-api-data-collector)

Includes the HHY entry point, pagination jobs, three adapters, incremental merge, configuration, and deterministic self-test.


## 15.2 Project layout

![HHY Multi-API Data Collector project tree](https://hhylang.dev/multi-api-data-collector-tree.png)

_collector.hhy orchestrates; jobs.hhy creates pages; sources.hhy downloads and normalizes; merge.hhy deduplicates and incrementally merges._


| File | Responsibility |
| --- | --- |
| collector.hhy | Compose collection, statistics, and atomic output |
| lib/jobs.hhy | Create OpenAlex, Crossref, and GitHub page jobs |
| lib/sources.hhy | Pacing, timeout, retry, concurrent download, normalization |
| lib/merge.hhy | Read old CSV, deduplicate, sort, and incrementally replace |


## 15.3 Run and incremental result

```sh
./build/hhy run practical-projects/multi-api-data-collector/collector.hhy \
  practical-projects/multi-api-data-collector/config/public-apis.json \
  practical-projects/multi-api-data-collector/output/records.csv \
  practical-projects/multi-api-data-collector/output/report.json \
  practical-projects/multi-api-data-collector/output/failures.json
```


![HHY Multi-API Data Collector terminal verification](https://hhylang.dev/multi-api-data-collector-run.png)

_Deterministic end-to-end verification: six pages, twelve incoming records, and nine unique records after both the initial and incremental runs._


{% hint style="info" %}
Public APIs and quotas change. Source URLs remain in the output; local fixtures exist only for regression testing and never enter the production output directory.
{% endhint %}


## 15.4 Verification

```sh
sh practical-projects/multi-api-data-collector/self-test.sh
```


The test runs twice to verify pagination, concurrency, normalization, cross-page deduplication, stable sorting, the failure list, and incremental replacement.
