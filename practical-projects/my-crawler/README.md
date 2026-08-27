# HHY Collector Framework

A Flow-first, auditable, resource-bounded framework for APIs and static documents. HHY owns HTTP, retry, bounded concurrency, failure reporting, and atomic output; the official `html` process extension owns CSS-selector extraction.

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

Edit `config/hhylang.json`. `seeds` provides entry pages, `parallelism` is bounded concurrency, `root_selector` selects one record root, and `schema` declares fields. Fields support `text`, `html`, and `attr`, plus `all`; an empty selector reads the record root itself.

The current boundary is deliberate: response bodies are buffered and the framework targets APIs and static HTML. It does not execute JavaScript or bypass robots.txt, authentication, or anti-bot controls. Crawl only sites you are authorized to access, identify the client, and keep concurrency conservative.

## Verify

```sh
./practical-projects/my-crawler/self-test.sh
```

The self-test uses a local fixture server and does not depend on the public internet.
