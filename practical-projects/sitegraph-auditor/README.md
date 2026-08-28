# SiteGraph Auditor

SiteGraph Auditor is a site-structure and content-quality gate built on the HHY
1.1.4 safe static spider. It recursively inventories authorized documentation,
builds a normalized link graph, archives failures, and returns a stable gate
exit status.

It exercises relative URL normalization, HTML link discovery, a bounded depth
frontier, domain/path/depth/page/frontier limits, pre-admission fingerprint
deduplication, and connection-level SSRF protection. Its deterministic test runs
both a four-level healthy site and a deliberately risky site with missing
metadata, a 404, an out-of-scope path, duplicate references, and an external
link.

Normal runs reuse HHY's unified user extension home (`~/.hhy/extensions`) and
do not keep a plug-in copy inside the project. CI and self-tests install into a
temporary `HHY_EXTENSION_HOME` that is removed after the run.

```sh
make
./practical-projects/sitegraph-auditor/self-test.sh
```
