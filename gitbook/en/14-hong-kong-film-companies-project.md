# 14. Project: Hong Kong Film Companies

Search MediaWiki for Hong Kong film-company pages, fetch details concurrently, filter the results, and export CSV plus JSON.

## 14.1 Real network research with HHY

This project uses HHY v1.2.0 with the official MediaWiki API. It searches for ten pages related to “香港電影公司”, fetches page IDs, canonical URLs, timestamps, and bounded introductions with parallel(3), retains entries that mention Hong Kong, film, and a company, sorts by title, and atomically writes CSV and JSON.


| Stage | HHY implementation |
| --- | --- |
| Search | http.get + timeout + retry + parse_json |
| Deduplicate | group_by(pageid) + Map |
| Details | parallel(3) + attempt so one page cannot stop peers |
| Filter | Stream + where + contains + sort_by |
| Output | encode_csv/encode_json + atomic save |


[View the complete source on GitHub ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/hong-kong-film-companies)

Includes the HHY crawler, configuration, modules, local MediaWiki fixture, CSV/JSON assertions, and bilingual guides.


## 14.2 Project layout

![Hong Kong film companies HHY Wikipedia research project tree](https://hhylang.dev/hong-kong-film-companies-tree.png)

_The real layout: crawl.hhy orchestrates, api.hhy handles search and concurrent details, and transform.hhy handles grouping, filtering, sorting, and report structures._


| File | Responsibility |
| --- | --- |
| crawl.hhy | Read configuration, compose the flow, and atomically write CSV/JSON |
| lib/api.hhy | MediaWiki search, HTTP policies, and bounded parallel work |
| lib/transform.hhy | Semantic filtering, stable sorting, and output fields |
| self-test.sh | Start a local fixture and verify the deterministic 5→3 result |


## 14.3 Actual Wikipedia result

On 2026-08-26, the actual run fetched all ten candidate details and retained seven company-related results: China Star Entertainment Group, Cathay Organisation, One Cool Film, Mandarin Films, Shaw Brothers, Film Workshop, and the Hong Kong film-company list.


```sh
./build/hhy run --limit max_runtime=2min --limit max_parallelism=8 \
  practical-projects/hong-kong-film-companies/crawl.hhy \
  practical-projects/hong-kong-film-companies/config/wikipedia.json \
  practical-projects/hong-kong-film-companies/output/wikipedia-report.json \
  practical-projects/hong-kong-film-companies/output/wikipedia-hong-kong-film-companies.csv
```


![Actual terminal result of HHY concurrently collecting Hong Kong film company data from Wikipedia](https://hhylang.dev/hong-kong-film-companies-run.png)

_Actual network run: ten candidates, ten fetched pages, seven retained companies, with CSV and JSON written successfully._


{% hint style="info" %}
Wikipedia search results and content change, so this is a reproducible research sample rather than an exhaustive business registry. The self-test uses a local API fixture so external network changes do not become code regressions.
{% endhint %}


## 14.4 Concurrency, limits, and repeatability

The default parallelism is three and each introduction is capped at 600 characters, bounding worker results. Requests use a ten-second timeout and two retries; attempt isolates each detail task. The Chinese keyword is percent-encoded in search_url to accommodate HHY 1.1.1's current non-ASCII query Map limitation.


```sh
sh practical-projects/hong-kong-film-companies/self-test.sh
```


[Read the English guide ↗](https://github.com/hh696-wq/hhy-vm/blob/main/practical-projects/hong-kong-film-companies/README.md)

See output fields, the real crawl command, configuration limits, and deterministic test design.
