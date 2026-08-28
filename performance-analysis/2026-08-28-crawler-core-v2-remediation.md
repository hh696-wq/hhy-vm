# HHY Crawler Core v2 implementation and verification report

Date: 2026-08-28
Release target: HHY 1.1.4

## Objective

Turn `practical-projects/my-crawler` from a bounded batch extractor into a safe
recursive static Spider with this minimum closed loop:

- URL normalization and relative-reference resolution
- HTML link discovery
- a depth frontier
- domain, path, depth, page, frontier, and per-page link limits
- normalized request-fingerprint deduplication
- SSRF protection at the resolved connection address

## Implementation evidence

### Runtime URL contract

`url_resolve(String, String?) -> Map` uses libcurl's URL API and returns the
normalized URL, scheme, host, port, path, query, and fingerprint. It:

- accepts only HTTP and HTTPS;
- rejects embedded credentials and U+0000;
- resolves relative references against an absolute base;
- removes fragments, dot segments, and default ports;
- normalizes host case.

The deterministic `tests/valid/url.hhy` case covers relative resolution, a
dot segment, host case, HTTPS port 443, query preservation, fragment removal,
and rejection of file URLs and credential-bearing URLs.

### Connection-level SSRF protection

HTTP requests now accept `allow_private_networks: Bool`. When false, libcurl's
socket-open callback checks the address produced by DNS resolution for every
connection, including redirects. It rejects IPv4/IPv6 loopback, private,
link-local, carrier-grade NAT, benchmarking, unspecified, multicast, and
IPv4-mapped private destinations.

General HTTP retains the compatibility default. The official crawler explicitly
sets the option to false; only its local fixture enables it. The crawler
self-test also sends a separate safe-mode request to 127.0.0.1 and requires it
to fail with the private-network diagnostic.

### Recursive frontier

`lib/engine.hhy` now:

1. normalizes and validates seeds;
2. fetches one frontier depth with bounded parallelism;
3. extracts records and discovers `href` values through `follow_selector`;
4. resolves every reference against its source page;
5. applies domain and path allowlists;
6. deduplicates the normalized fingerprint before admission;
7. enforces maximum depth, pages, frontier entries, and links per page;
8. records discovery, duplicate, rejection, limit, and depth statistics.

### Deterministic acceptance fixture

The three-page local fixture includes:

- a relative URL;
- a `..` path that normalizes to an existing target;
- a fragment duplicate;
- a backlink duplicate;
- an external-domain link;
- a query-bearing depth-two page.

Expected result: three pages, three records, no page failures, one rejected
external URL, and duplicate fingerprints detected before frontier admission.

## Verification evidence

- Release build with strict C warnings: passed.
- Complete Runtime, parser, extension, and HTTP test suite: passed.
- ASan/UBSan debug suite: passed.
- Crawler three-level acceptance and negative SSRF test: passed.
- Website HHY examples and callable registry: 47 examples / 95 callables passed.
- ESLint and TypeScript: passed.
- Next.js production build: 54 static pages generated.
- Browser verification on
  `http://localhost:9100/zh/learn/html-crawler-framework`: meaningful content,
  all six capability labels present, no framework overlay, no console errors.
- Browser navigation from the tutorial to `/zh`: passed, no console errors.

## Current boundary

This release provides a safe in-memory static Spider. It does not yet provide
JavaScript rendering, robots.txt policy, per-host throttling, a persistent
frontier, or resume after interruption. Those are follow-up capabilities and
are not implied by this release.
