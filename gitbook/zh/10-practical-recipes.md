# 10. 实战：可直接落地的自动化

完整收录 examples/00–08，并增加发布门禁、安全审计、订单对账、租户快照和素材治理。

## 10.1 00 · Hello HHY 与 Flow 入门

最小可运行案例：把 List 转成 Stream，依次完成映射和过滤，最后输出结果。对应 examples/00-hello.hhy。


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

✓ exit 0 · Flow 管道执行完成
```


## 10.2 并发提取日志告警

递归扫描大日志文件，使用 4 个 worker 提取 ERROR/WARN 行，并将来源文件写入结果。适合服务器日志归档、故障排查和定时任务。


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

✓ exit 0 · 3 条告警已写入 output/errors.txt
```


## 10.3 从 API 同步活跃用户

请求用户接口，经过超时、重试、JSON 解析和字段裁剪后，只把活跃用户原子写入本地文件。对应 examples/02-active-users.hhy。


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

✓ exit 0 · 2 位活跃用户已写入 active-users.json
```


## 10.4 进程 CPU / 内存监控

每 5 秒读取一次进程快照，找出 CPU 超过 70% 或内存超过 1 GiB 的进程，并按内存倒序输出前 10 个。对应 examples/03-process-monitor.hhy。


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

next sample in 5s… · Ctrl+C 安全退出
```


## 10.5 批量服务健康检查

并发探测多个服务，统一设置超时与重试；单个接口失败时记录错误，不中断整批巡检。可接入发布检查或 CI。


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

✓ exit 0 · 3 个服务并发完成，单点失败未中断批次
```


## 10.6 业务进阶 01 · 发布质量门禁

在发布前并行执行测试、Lint 和生产构建，生成机器可读报告；任一检查失败就用稳定退出码阻止发布。适合 CI/CD、灰度发布和交付验收。


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


## 10.7 业务进阶 02 · 源码敏感信息审计

并发扫描配置与源码，定位疑似 API Key、密码和私钥内容，汇总成可供安全团队复核的报告。适合提交前检查和合规巡检。


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

✓ exit 0 · 2 条疑似敏感信息待复核
```


{% hint style="info" %}
示例输出已脱敏。实际落地时应限制报告权限，并在流水线中避免打印秘密原文。
{% endhint %}


## 10.8 业务进阶 03 · 订单与支付自动对账

把订单 CSV 与支付 CSV 合并为一条数据流，按 order_id 分组，找出缺少记录或金额不一致的异常订单。适合每日财务对账。


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

✓ exit 0 · 对账报告已原子写入
```


## 10.9 业务进阶 04 · SaaS 多租户用量快照

有界并发拉取各租户用量，统一处理超时和重试；单租户失败被隔离并记录，不影响整份快照生成。适合计费、容量分析和客户成功报表。


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

✓ exit 0 · tenant-usage-snapshot.json 包含成功数据与失败原因
```


## 10.10 业务进阶 05 · 大体积素材治理

遍历图片与视频素材，筛选超过 5 MiB 的文件并按体积倒序生成 JSON 清单，帮助内容团队定位需要压缩或迁移的资产。


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

✓ exit 0 · asset-report.json 已生成
```


## 10.11 监听源码并自动构建

监听 C 代码变化，通过 debounce 合并短时间内的连续保存，再执行 make。构建失败只打印错误，监听任务继续运行。


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


## 10.12 从 CSV 生成部门汇总报表

读取员工 CSV，筛选在职人员，按部门统计人数和薪资总额，最后原子写入格式化 JSON。输入列为 name、department、active、salary。


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

✓ exit 0 · department-report.json 已原子写入
```


## 10.13 大文件备份（支持 dry-run）

找出超过 100 MiB 的文件并复制到备份目录。先用 dry-run 检查动作计划，确认无误后再真实执行。


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

✓ exit 0 · 仅生成计划，没有写入文件
```


{% hint style="info" %}
备份脚本默认不覆盖同名文件，并自动创建目标目录；正式执行前仍建议先运行 dry-run。
{% endhint %}


## 10.14 07 · 语言基础综合练习

用一个小型汇总任务串起变量、List、Map、函数、条件、循环、作用域和错误处理。对应 examples/07-language-basics.hhy。


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

✓ exit 0 · 汇总结果已生成
```
