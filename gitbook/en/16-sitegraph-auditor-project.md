# 16. Project: SiteGraph Auditor

Recursively inventory a documentation site, build a normalized link graph, and enforce metadata, failure, and security gates.

## 16.1 A quality gate that uses the safe spider end to end

SiteGraph Auditor builds on HHY v1.2.0 and the safe recursive my-crawler engine. Starting from seeds, it discovers pages by depth and writes an inventory, normalized graph, report, and failures. A stable exit status blocks sites with missing metadata or crawl failures.


| New capability | Use in this project |
| --- | --- |
| URL normalization | Unify relative links, dot segments, fragments, host case, and default ports |
| Link discovery | main a[href] continuously feeds the next frontier |
| Frontier | Concurrent depth batches retain page, depth, and source context |
| Hard boundaries | Domain, path, depth, pages, frontier, and links |
| Fingerprint deduplication | Deduplicate before frontier admission; count duplicate graph edges separately |
| SSRF | Production configuration rejects private resolved socket addresses across DNS and redirects |


[View the complete SiteGraph Auditor source ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/sitegraph-auditor)

Includes three HHY modules, a four-level healthy site, a risky site, report assertions, and a negative SSRF test.


## 16.2 Healthy and risky scenarios

```sh
make
./practical-projects/sitegraph-auditor/self-test.sh
```


```console
$ ./practical-projects/sitegraph-auditor/self-test.sh
SiteGraph Auditor healthy
Pages 4 / 4 Edges 5
Duplicates 3 Rejected 1 Findings 0
SiteGraph Auditor risky
Pages 1 / 2 Edges 2
Duplicates 0 Rejected 1 Findings 3
SiteGraph Auditor self-test passed
```


The healthy site has four levels, relative URLs, dot segments, fragment duplicates, and an external reference. The risky site has missing description/canonical metadata, a 404, and an out-of-scope path. A final request proves safe mode rejects loopback.


## 16.3 Outputs and boundary

| Output | Content |
| --- | --- |
| inventory.json | title, description, canonical, heading, and source_url |
| graph.json | source, raw href, normalized target, fingerprint, allowed state, and rejection reason |
| report.json | Pages, edges, duplicates, rejections, limits, errors, warnings, and findings |
| failures.json | URL, depth, and stable error |


{% hint style="info" %}
The default mode remains a static-site audit and does not bypass authentication, CAPTCHAs, robots.txt, or anti-bot controls. v1.1.5 can use an atomic checkpoint frontier with strict resume, plus an isolated Playwright renderer when JavaScript execution is required.
{% endhint %}
