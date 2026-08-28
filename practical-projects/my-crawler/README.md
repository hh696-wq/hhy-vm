# HHY Collector Framework

A Flow-first, auditable, resource-bounded static spider. HHY owns URL normalization, the frontier, HTTP, retry, bounded concurrency, failure reporting, and atomic output; the official `html` process extension owns link discovery and CSS-selector extraction.

## Initialize and run a real task

From the repository root:

```sh
make
./practical-projects/my-crawler/init.sh
./practical-projects/my-crawler/run.sh
```

The default task fetches `https://hhylang.dev/zh/learn/cli-reference`, extracts its level-two headings, and writes `output/records.json`, `output/report.json`, and `output/failures.json`.

`init.sh` installs the extension in the project's own `.hhy-extensions` directory, leaving the user-level extension home untouched. It is safe to run repeatedly.

## Configure a spider

Edit `config/hhylang.json`. `follow_selector` discovers links; domain, path, depth, page, and frontier limits constrain the crawl. `url_resolve` resolves relative references, removes fragments/default ports, normalizes dot segments and host case, and returns the request fingerprint used before frontier admission.

Crawler requests set `allow_private_networks: false`. The Runtime checks the resolved IPv4/IPv6 address for every actual libcurl connection, including redirects, and rejects private, loopback, and link-local destinations. Local fixtures opt in explicitly.

The current boundary is deliberate: response bodies are buffered and the framework targets APIs and static HTML. It does not execute JavaScript or bypass robots.txt, authentication, or anti-bot controls. Crawl only sites you are authorized to access, identify the client, and keep concurrency conservative.

## Verify

```sh
./practical-projects/my-crawler/self-test.sh
```

The three-level local fixture verifies normalization, discovery, frontier traversal, duplicate fingerprints, domain rejection, limits, extraction, and an actual blocked loopback request. It does not depend on the public internet.
