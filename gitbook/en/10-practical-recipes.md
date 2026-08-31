# 10. Practical Automation Recipes

Every example from examples/00–08, plus release gates, security audits, reconciliation, tenant snapshots, and asset governance.

## 10.1 00 · Hello HHY and Flow

The smallest runnable example: turn a List into a Stream, map and filter values, then print the result. Corresponds to examples/00-hello.hhy.


**00-hello.hhy**

```hhy
let language = "HHY"

["Flow", "Pipe", "System"]
    |> map { word -> "{language}: {word}" }
    |> print
```


```console
$ hhy run examples/00-hello.hhy
HHY: Flow
HHY: Pipe
HHY: System

✓ exit 0 · Flow pipeline completed
```


## 10.2 Extract log alerts concurrently

Recursively scan large log files with four workers, extract ERROR/WARN lines, and retain each source path. Useful for incident response and scheduled log jobs.


**log-errors.hhy**

```hhy
if length(args) != 2 {
    print_error("usage: hhy run log-errors.hhy <log-dir> <output-file>")
    exit(3)
}

let log_dir = path(args[0])
let output_file = path(args[1])

log_dir
    |> files("**/*.log")
    |> where { file -> file.size > 1mib }
    |> parallel(4) { file ->
    read_lines(file.path)
        |> where { line -> regex_match(line, /ERROR|WARN/) }
        |> map { line -> "{file.path}: {line}" }
        |> collect
}
    |> flat_map { lines -> lines |> stream }
    |> save_lines(output_file)
    |> on_error { err ->
    print_error(err)
    throw(err)
}
```


```sh
hhy run log-errors.hhy ./logs ./output/errors.txt
```


```console
$ hhy run log-errors.hhy ./logs ./output/errors.txt && head -3 ./output/errors.txt
logs/api.log: 2026-08-25T09:18:42Z ERROR database timeout after 3000ms
logs/worker.log: 2026-08-25T09:18:44Z WARN retrying job #1842
logs/api.log: 2026-08-25T09:18:47Z ERROR upstream returned 502

✓ exit 0 · 3 alerts written to output/errors.txt
```


## 10.3 Sync active users from an API

Fetch users with timeout and retry, parse JSON, select fields, and atomically save only active users. Corresponds to examples/02-active-users.hhy.


**active-users.hhy**

```hhy
if length(args) != 2 {
    print_error("usage: hhy run active-users.hhy <url> <output-file>")
    exit(3)
}

http.get(args[0])
    |> timeout(5s)
    |> retry({ count: 3, backoff: 200ms })
    |> send
    |> response_body
    |> parse_json
    |> get("users")
    |> stream
    |> where { user -> user.active == true }
    |> map { user ->
    { id: user.id, name: user.name, email: user.email }
}
    |> collect
    |> encode_json({ pretty: true })
    |> save_text(path(args[1]))
    |> on_error { err ->
    print_error(err)
    throw(err)
}
```


```sh
hhy run active-users.hhy https://api.example.com/users active-users.json
```


```console
$ hhy run active-users.hhy http://127.0.0.1:9000/users active-users.json && cat active-users.json
[
  { "id": 101, "name": "Ada", "email": "ada@example.com" },
  { "id": 108, "name": "Linus", "email": "linus@example.com" }
]

✓ exit 0 · 2 active users written to active-users.json
```


## 10.4 Monitor process CPU and memory

Sample processes every five seconds, keep CPU-heavy or memory-heavy entries, and print the top ten by memory. Corresponds to examples/03-process-monitor.hhy.


**03-process-monitor.hhy**

```hhy
every(5s)
    |> for_each { tick ->
    processes
        |> where { process ->
        process.cpu > 70% or process.memory > 1gib
    }
        |> sort_by({ order: "desc" }) { process -> process.memory }
        |> take(10)
        |> map { process ->
        {
            pid: process.pid,
            name: process.name,
            cpu: process.cpu,
            memory: process.memory
        }
    }
        |> print
}
```


```console
$ hhy run examples/03-process-monitor.hhy
[{ pid: 8421, name: "node", cpu: 82.4%, memory: 1.42 GiB },
 { pid: 9107, name: "hhy",  cpu: 74.1%, memory: 86.3 MiB }]

next sample in 5s… · Ctrl+C exits safely
```


## 10.5 Check service health in batches

Probe multiple services concurrently with consistent timeouts and retries. A failed endpoint is recorded without terminating the whole batch.


**health-check.hhy**

```hhy
let services = [
    { name: "users", url: "https://api.example.com/users/health" },
    { name: "orders", url: "https://api.example.com/orders/health" },
    { name: "billing", url: "https://api.example.com/billing/health" }
]

services
    |> stream
    |> parallel(3) { service ->
    let response = attempt {
        http.get(service.url)
            |> timeout(3s)
            |> retry({ count: 2, backoff: 100ms })
            |> send
            |> response_body
            |> parse_json
    }

    let mut status = "unreachable"
    let mut error_message = null

    if response.ok {
        status = response.value.status
    } else {
        error_message = response.error.message
    }

    return {
        name: service.name,
        ok: response.ok,
        status: status,
        error: error_message
    }
}
    |> collect
    |> encode_json({ pretty: true })
    |> print
```


```console
$ hhy run health-check.hhy
[
  { "name": "users",   "ok": true,  "status": "healthy", "error": null },
  { "name": "orders",  "ok": true,  "status": "healthy", "error": null },
  { "name": "billing", "ok": false, "status": "unreachable", "error": "request timed out" }
]

✓ exit 0 · all 3 checks completed despite one endpoint failure
```


## 10.6 Business 01 · Release quality gate

Run tests, lint, and production builds in parallel, save a machine-readable report, and block a release with a stable exit code when any check fails.


**release-gate.hhy**

```hhy
let checks = [
    { name: "unit-tests", command: ["make", "test"] },
    { name: "lint", command: ["npm", "run", "lint"] },
    { name: "production-build", command: ["npm", "run", "build"] }
]

let report = checks
    |> stream
    |> parallel(3) { check ->
    let result = run(check.command, { timeout: 10min, max_output: 8mib })
    return {
        name: check.name,
        passed: result.exit_code == 0,
        exit_code: result.exit_code,
        output: result.stdout
    }
}
    |> collect

report |> encode_json({ pretty: true }) |> save_text(path("release-gate.json"))

if report |> any { check -> check.passed == false } {
    print_error("release blocked: one or more checks failed")
    exit(1)
}

print("release gate passed")
```


```console
$ hhy run release-gate.hhy
unit-tests       PASS  4.28s
lint             PASS  1.14s
production-build PASS  6.72s
release-gate.json written

✓ exit 0 · release gate passed
```


## 10.7 Business 02 · Source secret audit

Scan configuration and source files concurrently for suspected API keys, passwords, and private keys, then produce a security review report.


**secret-audit.hhy**

```hhy
if length(args) != 2 {
    print_error("usage: hhy run secret-audit.hhy <source-dir> <report-file>")
    exit(3)
}

path(args[0])
    |> files("**/*")
    |> where { file ->
    file.extension == ".env" or
    file.extension == ".yml" or
    file.extension == ".json" or
    file.extension == ".ts"
}
    |> parallel(4) { file ->
    read_lines(file.path)
        |> where { line ->
        regex_match(line, /API_KEY|SECRET|PASSWORD|BEGIN PRIVATE KEY/)
    }
        |> map { line -> "{file.path}: {line}" }
        |> collect
}
    |> flat_map { matches -> matches |> stream }
    |> save_lines(path(args[1]))
```


```console
$ hhy run secret-audit.hhy ./services secret-findings.txt
services/billing/.env: PAYMENT_API_KEY=***
services/auth/config.yml: PASSWORD: ***

✓ exit 0 · 2 suspected secrets require review
```


{% hint style="info" %}
The example output is redacted. Restrict report access and avoid printing raw secrets in production pipelines.
{% endhint %}


## 10.8 Business 03 · Order and payment reconciliation

Merge order and payment CSV files into one flow, group records by order_id, and report missing records or mismatched amounts.


**reconcile.hhy**

```hhy
if length(args) != 3 {
    print_error("usage: hhy run reconcile.hhy <orders.csv> <payments.csv> <report.json>")
    exit(3)
}

[path(args[0]), path(args[1])]
    |> stream
    |> flat_map { input ->
    read_lines(input) |> parse_csv({ header: true })
}
    |> group_by { record -> record.order_id }
    |> where { group ->
    (group.values |> count) != 2 or
    (group.values |> map { record -> record.amount } |> distinct |> count) != 1
}
    |> map { group ->
    { order_id: group.key, records: group.values, issue: "missing_or_amount_mismatch" }
}
    |> collect
    |> encode_json({ pretty: true })
    |> save_text(path(args[2]))
```


```console
$ hhy run reconcile.hhy orders.csv payments.csv exceptions.json
orders: 12,480 · payments: 12,472
matched: 12,461
exceptions: 19 → exceptions.json

✓ exit 0 · reconciliation report written atomically
```


## 10.9 Business 04 · Multi-tenant usage snapshot

Fetch tenant usage with bounded concurrency, consistent retries, and failure isolation. Useful for billing, capacity analysis, and customer success reports.


**tenant-snapshot.hhy**

```hhy
let tenants = [
    { id: "acme", url: "https://api.example.com/acme/usage" },
    { id: "nova", url: "https://api.example.com/nova/usage" },
    { id: "orbit", url: "https://api.example.com/orbit/usage" }
]

tenants
    |> stream
    |> parallel(3) { tenant ->
    let result = attempt {
        http.get(tenant.url)
            |> timeout(5s)
            |> retry({ count: 3, backoff: 200ms })
            |> send
            |> response_body
            |> parse_json
    }

    if result.ok {
        return { tenant: tenant.id, ok: true, usage: result.value, error: null }
    }

    return { tenant: tenant.id, ok: false, usage: null, error: result.error.message }
}
    |> collect
    |> encode_json({ pretty: true })
    |> save_text(path("tenant-usage-snapshot.json"))
```


```console
$ hhy run tenant-snapshot.hhy
acme  ✓ requests=184203 storage_gb=82.4
nova  ✓ requests=99102  storage_gb=41.8
orbit ✗ request timed out

✓ exit 0 · snapshot contains both data and failure reasons
```


## 10.10 Business 05 · Oversized asset governance

Find large image and video assets, sort them by size, and produce a JSON inventory for compression or storage migration work.


**asset-audit.hhy**

```hhy
if length(args) != 2 {
    print_error("usage: hhy run asset-audit.hhy <asset-dir> <report.json>")
    exit(3)
}

path(args[0])
    |> files("**/*")
    |> where { file ->
    file.is_file and
    (file.extension == ".png" or file.extension == ".jpg" or file.extension == ".mp4")
}
    |> where { file -> file.size > 5mib }
    |> sort_by({ order: "desc" }) { file -> file.size }
    |> map { file ->
    { path: file.path, bytes: file.size, extension: file.extension }
}
    |> collect
    |> encode_json({ pretty: true })
    |> save_text(path(args[1]))
```


```console
$ hhy run asset-audit.hhy ./public asset-report.json
scanned 1,842 assets
large assets: 27
largest: public/video/launch.mp4 · 184.2 MiB

✓ exit 0 · asset-report.json generated
```


## 10.11 Watch sources and rebuild

Watch C sources, debounce rapid saves, and run make. Build failures are reported while the watcher stays alive.


**watch-build.hhy**

```hhy
if length(args) != 1 {
    print_error("usage: hhy run watch-build.hhy <source-dir>")
    exit(3)
}

let source_dir = path(args[0])

watch(source_dir, { recursive: true })
    |> where { event ->
    event.kind != "removed" and
    (event.path.extension == ".c" or event.path.extension == ".h")
}
    |> debounce(300ms)
    |> for_each { event ->
    print("changed: {event.path}")

    let result = run(["make"], { timeout: 2min, cwd: system.cwd })

    if result.exit_code != 0 {
        print_error(result.stderr)
    } else {
        print(result.stdout)
    }
}
```


```sh
hhy run watch-build.hhy ./src
```


```console
$ hhy run watch-build.hhy ./src
watching ./src recursively…
changed: src/runtime/flow.c
cc -std=c11 -O2 -c src/runtime/flow.c
cc build/*.o -lcurl -lpcre2-8 -lgc -o build/hhy
Build complete: build/hhy

✓ watcher remains active · waiting for the next change
```


## 10.12 Build a department report from CSV

Read employee CSV records, keep active employees, aggregate headcount and salary by department, and atomically save formatted JSON.


**csv-report.hhy**

```hhy
if length(args) != 2 {
    print_error("usage: hhy run csv-report.hhy <input.csv> <output.json>")
    exit(3)
}

read_lines(path(args[0]))
    |> parse_csv({ header: true })
    |> where { employee -> employee.active == "true" }
    |> group_by { employee -> employee.department }
    |> map { group ->
    {
        department: group.key,
        employees: group.values |> count,
        total_salary: group.values
            |> map { employee -> employee.salary |> to_float }
            |> sum
    }
}
    |> collect
    |> encode_json({ pretty: true })
    |> save_text(path(args[1]))
```


```sh
hhy run csv-report.hhy employees.csv department-report.json
```


```console
$ hhy run csv-report.hhy employees.csv department-report.json && cat department-report.json
[
  { "department": "Engineering", "employees": 12, "total_salary": 2160000 },
  { "department": "Product", "employees": 5, "total_salary": 810000 }
]

✓ exit 0 · department-report.json written atomically
```


## 10.13 Back up large files with dry-run

Find files over 100 MiB and copy them into a backup directory. Inspect the plan with dry-run before performing real writes.


**backup-large.hhy**

```hhy
if length(args) != 2 {
    print_error("usage: hhy run backup-large.hhy <source-dir> <backup-dir>")
    exit(3)
}

let source_dir = path(args[0])
let backup_dir = path(args[1])

source_dir
    |> files("**/*")
    |> where { file -> file.is_file and file.size > 100mib }
    |> for_each { file ->
    let target = path_join(backup_dir, file.name)
    print("copy {file.path} -> {target}")
    copy(file.path, target, { overwrite: false, create_parents: true })
}
    |> on_error { err ->
    print_error(err)
    throw(err)
}
```


```sh
hhy run --dry-run backup-large.hhy ./downloads ./backup
hhy run backup-large.hhy ./downloads ./backup
```


```console
$ hhy run --dry-run backup-large.hhy ./downloads ./backup
copy downloads/archive.tar -> backup/archive.tar
copy downloads/database.dump -> backup/database.dump
[dry-run] copy downloads/archive.tar → backup/archive.tar
[dry-run] copy downloads/database.dump → backup/database.dump

✓ exit 0 · plan generated without writing files
```


{% hint style="info" %}
The recipe refuses to overwrite files and creates parent directories, but you should still inspect the dry-run plan first.
{% endhint %}


## 10.14 07 · Language basics in one task

A small aggregation task combining variables, Lists, Maps, functions, conditions, loops, scopes, and error handling. Corresponds to examples/07-language-basics.hhy.


**07-language-basics.hhy**

```hhy
fn summarize(items) {
    let mut total = 0

    for item in items {
        if item.enabled {
            total = total + item.score
        }
    }

    return total
}

let users = [
    { name: "Ada", enabled: true, score: 98 },
    { name: "Linus", enabled: false, score: 86 }
]

summarize(users) |> print
```


```console
$ hhy run examples/07-language-basics.hhy
{
  "count": 2,
  "total": 40,
  "average": 20
}

✓ exit 0 · summary generated
```
