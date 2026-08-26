# Multi-API Data Collector

This practical project uses one HHY Flow to collect paginated records concurrently from OpenAlex, Crossref, and GitHub. It normalizes heterogeneous JSON, records failed pages, deduplicates and sorts records, and incrementally merges them into an atomic CSV output.

See [README.zh-CN.md](README.zh-CN.md) for the full guide.

Run the deterministic local verification with:

```sh
sh practical-projects/multi-api-data-collector/self-test.sh
```
