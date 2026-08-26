# DataFlow ETL

[中文说明](README.zh-CN.md)

A complete HHY v1.0 ETL application: CSV and JSON-directory ingestion, cleaning and filtering, concurrent HTTP enrichment, grouping, and atomic JSON/CSV outputs.

```sh
sh practical-projects/dataflow-etl/self-test.sh
```

The deterministic test expects three qualified customers, two event files, complete HTTP enrichment, two department summaries, and matching JSON/CSV output.
