# 9. Modules and Errors

Organize code, propagate structured errors, and unwind resources reliably.

## 9.1 Import forms and path resolution

```hhy
import { add } from "./math.hhy"

add(20, 22) |> print
```


| Form | Use |
| --- | --- |
| import "./lib/report.hhy" as report | Import a local module namespace |
| import { parse } from "./lib/data.hhy" | Named import |
| import { validate as check } from "./lib/data.hhy" | Named import with alias |
| import http | Import a standard-library module |


Relative paths resolve from the current source file. Standard modules use bare names; local files explicitly use ./, ../, or an absolute Path.


## 9.2 Exports, scope, and execution

```hhy
export let version = "1.0"

export fn normalize_name(name) {
    return name |> trim |> lower
}

fn internal_helper() {
    return null
}
```


Only exported names are visible to importers. A module owns its top-level scope, executes once on first import, and is then cached. Import cycles raise CheckError before execution. V1.2.0 supports standard and local modules plus process-extension modules installed from local packages.


## 9.3 Error fields and categories

Every failure uses Error rather than null or printed text. Error exposes kind, code, message, source, stage, cause, stack, and context. Sensitive headers, credentials, and full file contents are excluded from context by default.


Built-in categories include SyntaxError, CheckError, TypeError, ValueError, IndexError, KeyError, EncodingError, IoError, ProcessError, HttpError, HttpStatusError, TimeoutError, CancelledError, ResourceLimitError, and PlanError.


## 9.4 try/catch and rethrowing

```hhy
try {
    read_text(path("config.json"))
        |> parse_json
        |> print
} catch err {
    print_error(err)
    exit(1)
}
```


catch receives the first Error propagated from try. Execution continues after a normally completed catch; use throw(err) to preserve the error chain when it cannot be handled. An unhandled Error exits nonzero.


## 9.5 Flow errors and per-item Result

```hhy
path("./configs")
    |> files("**/*.json")
    |> map { file -> attempt { read_text(file.path) } }
    |> where { result -> result.ok }
    |> map { result -> result.value }
    |> print
```


An unhandled Error in a Stream terminates the Pipeline. attempt converts one operation to Result for batches that explicitly retain successes and failures. on_error replaces an entirely failed upstream Stream and never silently skips an item.


## 9.6 Resource cleanup guarantees

Error, return, exit, timeout, Ctrl+C, and cancel share one unwind path. Stream close is idempotent; failed atomic saves delete temporary files and preserve the old file; child processes, HTTP responses, watchers, and workers respond to cancellation.
