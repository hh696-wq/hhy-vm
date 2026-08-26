# Hong Kong Film Companies: concurrent Wikipedia research with HHY

This project uses HHY and the official MediaWiki API to search for `香港電影公司`, fetch page introductions concurrently, retain company-related results, and atomically write a CSV dataset plus a JSON run report.

It demonstrates HTTP query options and headers, timeout/retry/attempt error handling, bounded `parallel(3)` work, Stream grouping, filtering and stable sorting, JSON/CSV encoding, and atomic files.

## Run against Wikipedia

From the repository root:

```sh
./build/hhy run \
  practical-projects/hong-kong-film-companies/crawl.hhy \
  practical-projects/hong-kong-film-companies/config/wikipedia.json \
  practical-projects/hong-kong-film-companies/output/report.json \
  practical-projects/hong-kong-film-companies/output/hong-kong-film-companies.csv
```

The result is a dynamic research sample rather than an exhaustive company registry. Wikipedia search results and page content can change between runs.

Every CSV row retains its Wikipedia URL for verification and attribution. Wikipedia text is generally available under CC BY-SA; follow the license and attribution notices on the relevant pages before redistributing a CSV that contains extracts.

The Chinese keyword in `config/wikipedia.json` is percent-encoded in `search_url` to work around HHY 1.1.0's current non-ASCII query Map encoding limitation. ASCII detail-request parameters are still constructed through HHY's query Map.

## Deterministic self-test

```sh
sh practical-projects/hong-kong-film-companies/self-test.sh
```

The local fixture verifies concurrent detail fetching, semantic filtering, stable title ordering, CSV fields, URLs, and JSON statistics without depending on external network availability.
