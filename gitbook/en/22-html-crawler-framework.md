# 22. HTML Extension and Crawler Framework

Combine the official HTML extension with a safe frontier for URL normalization, discovery, recursive deduplication, and bounded static crawling.

## 22.1 What the HTML extension is

The official html 0.1.0 package is a process extension with no network or filesystem effects. Lexbor parses untrusted HTML, evaluates CSS selectors, and returns String, Bool, List, and Map values directly usable by HHY.


{% hint style="info" %}
The extension parses and extracts only. It does not fetch URLs, schedule pages, or return DOM handles. HHY Runtime and the crawler layer own HTTP, TLS, timeouts, retries, and security policy.
{% endhint %}


## 22.2 Build and install the HTML extension

```sh
brew install jansson lexbor
make -C extensions/html
./build/hhy install ./extensions/html
./build/hhy list
```


| Complete signature | Purpose |
| --- | --- |
| html.text(String html, String selector, Map?) -> String? | Read normalized text from the first matching node |
| html.text_all(String html, String selector, Map?) -> List<String> | Read text from every matching node |
| html.attr(String html, String selector, String name, Map?) -> String? | Read an attribute from the first matching node |
| html.attr_all(String html, String selector, String name, Map?) -> List<String> | Read an attribute from every matching node |
| html.exists(String html, String selector) -> Bool | Test whether a selector matches |
| html.extract(String html, String selector, Map schema, Map?) -> List<Map> | Parse once and project repeated records through a schema |


| Option | Callables | Behavior |
| --- | --- | --- |
| trim: Bool | text, text_all, attr, attr_all | Trim surrounding whitespace; enabled by default |
| max_results: Int | text_all, attr_all, extract | Defaults to 1000; hard limit 10000 |


The extension limits input to 768 KiB, keeping protocol messages bounded. extract schema fields use { selector, value: "text" } or { selector, value: "attr", name }; an empty selector reads the current root. It does not return DOM handles: Protocol 1 transports only JSON-shaped values, so html.extract performs parsing and projection inside one extension call.


## 22.3 From HTML extraction to a safe crawler

The static spider composes separate responsibilities: Runtime url_resolve normalizes relative references; the HTML extension discovers links; a depth frontier checks domains, paths, page and queue budgets, and fingerprints before admission; the HTTP layer blocks SSRF at the resolved connection address.


| Capability | Current implementation |
| --- | --- |
| url_resolve(url, base?) | Returns url, scheme, host, port, path, query, and a stable fingerprint |
| Link discovery | follow_selector extracts href values and feeds the next frontier |
| Frontier and deduplication | Depth batches with bounded concurrency and pre-admission fingerprint deduplication |
| Hard boundaries | Domains, path prefixes, depth, pages, frontier size, and links per page |
| SSRF protection | Reject loopback, private, and link-local socket addresses, including redirects |


## 22.4 Project one: the my-crawler foundation

```sh
make
./practical-projects/my-crawler/init.sh
./practical-projects/my-crawler/self-test.sh
./practical-projects/my-crawler/run.sh
```


my-crawler is the smallest runnable spider: it reads JSON configuration, recursively discovers authorized links, extracts records, and writes report and failure artifacts. Normal runs reuse ~/.hhy/extensions; CI and self-tests use a temporary HHY_EXTENSION_HOME.


**config/hhylang.json**

```text
{
  "seeds": ["https://hhylang.dev/zh/learn/cli-reference"],
  "allowed_domains": ["hhylang.dev"],
  "allowed_path_prefixes": ["/zh/learn/"],
  "follow_selector": "main article a[href]",
  "parallelism": 2,
  "max_depth": 2,
  "max_pages": 50,
  "max_frontier": 100,
  "max_links_per_page": 200,
  "allow_private_networks": false,
  "root_selector": "main article h2",
  "max_results": 100,
  "schema": { "title": { "selector": "", "value": "text" } }
}
```


```console
$ ./practical-projects/my-crawler/self-test.sh
HHY Collector Framework Crawler Fixture
Pages 3 / 3 Records 3 Failures 0
HHY Collector Framework self-test passed
```


[Read the complete my-crawler source](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/my-crawler)

Start here for configuration, recursive crawling, structured extraction, failure archives, and deterministic tests.


## 22.5 Project two: the SiteGraph Auditor challenge

SiteGraph Auditor adds a page inventory, normalized graph, metadata audit, and CI quality gate on top of the foundational spider, with both healthy and risky fixtures.


| Output | Content |
| --- | --- |
| inventory.json | Page metadata, primary heading, and source URL |
| graph.json | Normalized edges, fingerprints, allowed state, and rejection reason |
| report.json | Pages, edges, duplicates, limits, errors, warnings, and findings |
| failures.json | Failed URL, depth, and stable error |


[Open the SiteGraph Auditor challenge](/en/learn/sitegraph-auditor-project)

Continue with a site graph, content-quality audit, negative SSRF test, and CI gate.


## 22.6 Scope and explicit boundaries

The crawler supports an in-memory or atomically checkpointed frontier, strict configuration-matched resume, and send_to response files. An optional Playwright renderer executes JavaScript separately from the side-effect-free Lexbor extension and applies domain plus DNS private-network checks to documents, redirects, and subresources.


{% hint style="info" %}
Do not use it to bypass robots.txt, authentication, CAPTCHAs, or anti-bot controls. Crawl only authorized sites with an identifiable User-Agent, conservative concurrency, and explicit page budgets.
{% endhint %}
