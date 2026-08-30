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

`init.sh` reuses HHY's unified user extension home (`~/.hhy/extensions`) by default. CI and tests can set `HHY_EXTENSION_HOME` for an isolated installation. It is safe to run repeatedly.

## Configure a spider

Edit `config/hhylang.json`. `follow_selector` discovers links; domain, path, depth, page, and frontier limits constrain the crawl. `url_resolve` resolves relative references, removes fragments/default ports, normalizes dot segments and host case, and returns the request fingerprint used before frontier admission.

Crawler requests set `allow_private_networks: false`. The Runtime checks the resolved IPv4/IPv6 address for every actual libcurl connection, including redirects, and rejects private, loopback, and link-local destinations. Local fixtures opt in explicitly.

With `response_directory`, Runtime `send_to` streams the transport body into an atomic file before the crawler reads it for HTML extraction. `checkpoint_path` persists the frontier, fingerprints, pages, and statistics after each batch; `resume: true` restores only a version- and configuration-matched checkpoint.

Optional JavaScript rendering uses the separately installed, version-locked Playwright adapter in `renderer/`. Every document, redirect, and subresource remains subject to the domain and DNS private-network policy. It does not bypass robots.txt, authentication, CAPTCHAs, or anti-bot controls.

## Verify

```sh
./practical-projects/my-crawler/self-test.sh
```

The local fixture verifies three-level traversal, streamed response files, atomic checkpoint/resume, real JavaScript-generated content, and an actually blocked loopback request. It does not depend on the public internet.
