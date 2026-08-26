import type { Language } from "./i18n";
import { hhyVersion, hhyVersionLabel, hhyVersionTag } from "./release";

export type ChapterSlug =
  | "quick-start"
  | "language-basics"
  | "flow-and-streams"
  | "files-and-paths"
  | "text-json-csv"
  | "processes"
  | "http"
  | "parallel-watch"
  | "modules-errors"
  | "flowguard-project"
  | "dataflow-etl-project"
  | "asset-governance-project"
  | "syntax-reference"
  | "standard-library"
  | "extensions-roadmap"
  | "language-vm-roadmap"
  | "practical-recipes"
  | "cli-reference";

export type DocBlock =
  | { type: "p"; text: string }
  | { type: "note"; text: string }
  | { type: "code"; language: "hhy" | "sh" | "text"; code: string; filename?: string }
  | { type: "terminal"; command: string; output: string }
  | { type: "terminal-card"; title: string; command: string; output: string; caption: string }
  | { type: "list"; items: string[] }
  | { type: "table"; columns: string[]; rows: string[][] }
  | { type: "link"; href: string; label: string; description: string }
  | { type: "image"; src: string; alt: string; caption: string; width: number; height: number; size: "medium" | "wide" }
  | { type: "extension-flow" }
  | { type: "evolution-roadmap" }
  | { type: "api"; entries: { name: string; signature: string; description: string }[] };

export type DocSection = {
  title: string;
  blocks: DocBlock[];
};

export type Chapter = {
  slug: ChapterSlug;
  order: number;
  title: Record<Language, string>;
  summary: Record<Language, string>;
  sections: Record<Language, DocSection[]>;
};

const code = {
  hello: `let language = "HHY"

["Flow", "Pipe", "System"]
    |> map { word -> "{language}: {word}" }
    |> print`,
  basics: `fn summarize(items) {
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

summarize(users) |> print`,
  basicsValues: `let nothing = null
let enabled = true
let count = 42
let ratio = 0.75
let title = "HHY"
let pattern = /ERROR|WARN/i
let names = ["Ada", "Linus"]
let user = { name: "Ada", active: true }
let indexes = 0..3
let size_limit = 10mib
let timeout_limit = 5s
let completion = 80%

print(type(user))
print(is_type(title, "String"))`,
  basicsCollections: `let original = ["Flow", "System"]
let extended = append(original, "Pipe")
let shortened = remove_at(extended, 1)

let config = { retries: 3, label: null }
let updated = put(config, "timeout", 5s)
let selected = pick(updated, ["retries", "timeout"])

print(original)
print(shortened)
print(get(config, "missing"))
print(require(config, "label"))
print(selected)`,
  basicsControl: `fn classify(score) {
    if score >= 90 { return "excellent" }
    else if score >= 60 { return "pass" }
    else { return "retry" }
}

let mut total = 0
for score in [98, 72, 55] {
    if score < 60 { continue }
    total = total + score
}

let mut attempts = 0
while attempts < 3 {
    attempts = attempts + 1
}

print(classify(98))
print(total)`,
  flow: `[1, 2, 3, 4, 5]
    |> stream
    |> map { number -> number * 2 }
    |> where { number -> number > 5 }
    |> take(2)
    |> print`,
  flowOperators: `[5, 2, 5, 1, 3]
    |> stream
    |> skip(1)
    |> take(4)
    |> inspect { number -> print("seen {number}") }
    |> where { number -> number >= 3 }
    |> map { number -> number * 10 }
    |> distinct
    |> collect
    |> print`,
  flowFlatMap: `let batches = [[1, 2], [3, 4]]

batches
    |> stream
    |> flat_map { batch -> batch |> stream }
    |> print`,
  flowBarriers: `let ordered = [5, 1, 3, 2, 4]
    |> stream
    |> sort_by({ order: "asc" }) { number -> number }
    |> collect

let grouped = [
    { team: "core", name: "Ada" },
    { team: "web", name: "Linus" },
    { team: "core", name: "Grace" }
]
    |> stream
    |> group_by { person -> person.team }
    |> collect

print(ordered)
print(grouped)`,
  files: `path("./logs")
    |> files("**/*.log")
    |> where { file -> file.size > 1mib }
    |> flat_map { file -> read_lines(file.path) }
    |> where { line -> contains(line, "ERROR") }
    |> save_lines(path("errors.txt"))`,
  pathFields: `let source = path("./src/../src/main.c")
let target = path_join(source.parent, "runtime.c")

print(source)
print(source.name)
print(source.extension)
print(source.parent)
print(target)`,
  fileOperations: `let input = path("notes.txt")
let backup = path("backup/notes.txt")

write_text(input, "first line\n", { overwrite: true })
append_text(input, "second line\n")
copy(input, backup, { overwrite: false, create_parents: true })

read_lines(backup)
    |> map { line -> upper(line) }
    |> save_lines(path("backup/upper.txt"), { create_parents: true })`,
  json: `read_text(path("users.json"))
    |> parse_json
    |> get("users")
    |> stream
    |> where { user -> user.active == true }
    |> collect
    |> encode_json({ pretty: true })
    |> save_text(path("active-users.json"))`,
  process: `run(["git", "log", "--oneline"], { timeout: 5s })
    |> stdout_lines
    |> take(10)
    |> print`,
  http: `http.get("https://example.com/users")
    |> timeout(5s)
    |> retry({ count: 3, backoff: 200ms })
    |> send
    |> response_body
    |> parse_json
    |> print`,
  parallel: `let urls = [
    "https://example.com",
    "https://example.org"
]

urls
    |> parallel(2) { url ->
    http.get(url)
        |> timeout(5s)
        |> send
}
    |> print`,
  watch: `watch(path("./src"))
    |> where { event -> event.kind == "write" }
    |> debounce(300ms)
    |> for_each { event ->
    print(event.path)
}`,
  errors: `try {
    read_text(path("config.json"))
        |> parse_json
        |> print
} catch err {
    print_error(err)
    exit(1)
}`,
  module: `import { add } from "./math.hhy"

add(20, 22) |> print`,
  practicalLogs: `if length(args) != 2 {
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
}`,
  practicalHealth: `let services = [
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
    |> print`,
  practicalActiveUsers: `if length(args) != 2 {
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
}`,
  practicalProcesses: `every(5s)
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
}`,
  practicalWatch: `if length(args) != 1 {
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
}`,
  practicalCsv: `if length(args) != 2 {
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
    |> save_text(path(args[1]))`,
  practicalBackup: `if length(args) != 2 {
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
}`,
  businessReleaseGate: `let checks = [
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

print("release gate passed")`,
  businessSecretAudit: `if length(args) != 2 {
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
    |> save_lines(path(args[1]))`,
  businessReconcile: `if length(args) != 3 {
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
    |> save_text(path(args[2]))`,
  businessTenantSnapshot: `let tenants = [
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
    |> save_text(path("tenant-usage-snapshot.json"))`,
  businessAssetAudit: `if length(args) != 2 {
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
    |> save_text(path(args[1]))`,
  cli: `hhy script.hhy [args...]
hhy run script.hhy [args...]
hhy repl
hhy check script.hhy...
hhy fmt script.hhy...
hhy fmt --check script.hhy...
hhy ast script.hhy
hhy tokens script.hhy
hhy run --dry-run script.hhy
hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
hhy --version
hhy --help`,
  syntaxLiterals: `let nothing = null
let flags = [true, false]
let numbers = [42, -10, 0xff, 0b1010, 1.5, 1e6]
let name = "HHY"
let strings = ["hello", "Hello, {name}"]
let pattern = /ERROR|WARN/i
let list = [1, 2, 3]
let record = { name: "Tom", age: 20 }
let interval = 1..10
let units = [10mib, 5s, 80%]`,
  syntaxOperators: `()  []  .
not  -  +
*  /  %
+  -
<  <=  >  >=
==  !=
and
or
??
|>
=`,
  syntaxStatements: `let name = "HHY"
let mut count = 0
count = count + 1
let enabled = true
let items = ["Flow", "Pipe"]

if enabled { print("yes") } else { print("no") }
for item in items { print(item) }
while count < 3 { count = count + 1 }

fn add(a, b) { return a + b }
let doubled = [1, 2] |> stream |> map { number -> number * 2 } |> collect

try { read_text(path("config.json")) } catch err { print_error(err) }
let result = attempt { read_text(path("config.json")) }

import { add as sum_two } from "./math.hhy"
export fn public_api(value) { return value }`,
  stdCore: `print(Value...) -> Null
print_error(Value...) -> Null
exit(Int?) -> Never
length(String | List | Map) -> Int
byte_length(String | BytesBuffer) -> Int
type(Value) -> String
is_type(Value, String) -> Bool
to_int(Int | Float | String) -> Int
to_float(Int | Float | String) -> Float
get(List | Map | Record, Int | String) -> Value | Null
require(Map, String) -> Value
pick(Map, List<String>) -> Map
put(Map, String, Value) -> Map
remove_key(Map, String) -> Map
append(List<T>, T) -> List<T>
remove_at(List<T>, Int) -> List<T>
now() -> DateTime
datetime.parse(String, String, String) -> DateTime
require_env(String) -> String
sleep(Duration) -> Null
cancel() -> Never
throw(Error) -> Never`,
  stdFlow: `stream(List<T> | Map | Range) -> Stream<T>
range(Int, Int) -> Stream<Int>
map(Stream<T>, Function(T -> U)) -> Stream<U>
flat_map(Stream<T>, Function(T -> Stream<U>)) -> Stream<U>
where(Stream<T>, Function(T -> Bool)) -> Stream<T>
take(Stream<T>, Int) -> Stream<T>
skip(Stream<T>, Int) -> Stream<T>
inspect(Stream<T>, Function(T -> Value)) -> Stream<T>
distinct(Stream<Hashable>) -> Stream<Hashable>
sort_by(Stream<T>, Map, Function(T -> Comparable)) -> Stream<T>
group_by(Stream<T>, Function(T -> Hashable)) -> Stream<Group<T>>
debounce(Stream<T>, Duration) -> Stream<T>
on_error(Stream<T>, Function(Error -> Stream<T>)) -> Stream<T>
parallel(Stream<T>, Int, Function(T -> U)) -> Stream<U>
collect(Stream<T>) -> List<T>
count(Stream<T>) -> Int
first(Stream<T>) -> T | Null
last(Stream<T>) -> T | Null
min(Stream<Number>) -> Number | Null
max(Stream<Number>) -> Number | Null
sum(Stream<Number>) -> Number
reduce(Stream<T>, U, Function(State<T,U> -> U)) -> U
any(Stream<T>, Function(T -> Bool)) -> Bool
all(Stream<T>, Function(T -> Bool)) -> Bool
for_each(Stream<T>, Function(T -> Value)) -> Null`,
  stdText: `contains(String | List, Value) -> Bool
upper(String) -> String
lower(String) -> String
trim(String) -> String
trim_start(String) -> String
trim_end(String) -> String
starts_with(String, String) -> Bool
ends_with(String, String) -> Bool
replace(String, String, String) -> String
split(String, String) -> List<String>
join(List<String>, String) -> String
regex_match(String, Regex) -> Bool
regex_captures(String, Regex) -> Map | Null
parse_json(String) -> JsonValue
encode_json(JsonValue, Map?) -> String
parse_csv(String | Stream<String>, Map?) -> Stream<Map>
encode_csv(Stream<Map>, Map?) -> Stream<String>`,
  stdFiles: `path(String) -> Path
path_join(Path, String | Path) -> Path
files(Path, String, Map?) -> Stream<File | Directory>
read_text(Path) -> String
read_lines(Path) -> Stream<String>
read_bytes(Path) -> BytesBuffer
write_text(Path, String, Map?) -> Path
append_text(Path, String) -> Path
write_bytes(Path, BytesBuffer, Map?) -> Path
save_text(String | Stream<String>, Path, Map?) -> Path
save_lines(Stream<String>, Path, Map?) -> Path
copy(Path, Path, Map?) -> Path
move(Path, Path, Map?) -> Path
remove(Path) -> Path
watch(Path, Map?) -> Stream<FileEvent>`,
  stdProcess: `run(List<String>, Map?) -> CommandResult
shell(String, Map?) -> CommandResult
stdout_lines(CommandResult) -> Stream<String>
processes() -> Stream<Process>
stdin_lines() -> Stream<String>
every(Duration) -> Stream<Int>`,
  stdHttp: `http.get(String, Map?) -> HttpRequest
http.post(String, Map?) -> HttpRequest
http.put(String, Map?) -> HttpRequest
http.delete(String, Map?) -> HttpRequest
timeout(HttpRequest, Duration) -> HttpRequest
retry(HttpRequest, Map) -> HttpRequest
send(HttpRequest) -> HttpResponse
response_body(HttpResponse) -> String
response_bytes(HttpResponse) -> BytesBuffer`
};

const callableDescriptions: Record<string, Record<Language, string>> = {
  print: { zh: "把值写到标准输出；传入 Stream 时逐项输出并消费它。", en: "Write values to stdout; a Stream is consumed and printed item by item." },
  print_error: { zh: "把值写到标准错误；适合诊断信息。", en: "Write values to stderr for diagnostics." },
  exit: { zh: "立即以给定状态码结束脚本，省略时使用 0，并触发资源清理。", en: "End the script with an optional status code (default 0) and unwind resources." },
  length: { zh: "返回 String 的 code point 数或 List/Map 的元素数；Stream 应使用 count。", en: "Return String code points or List/Map elements; use count for a Stream." },
  byte_length: { zh: "返回 String 的 UTF-8 字节数或 BytesBuffer 大小。", en: "Return the UTF-8 byte count of String or size of BytesBuffer." },
  type: { zh: "返回值的逻辑类型名。", en: "Return a value's logical type name." },
  is_type: { zh: "判断值是否具有指定逻辑类型，返回 Bool。", en: "Test whether a value has the named logical type." },
  to_int: { zh: "把 Int/Float/String 显式转换为 Int，失败或溢出产生 ValueError。", en: "Explicitly convert Int/Float/String to Int; invalid or overflowing input raises ValueError." },
  to_float: { zh: "把 Int/Float/String 显式转换为 Float，失败产生 ValueError。", en: "Explicitly convert Int/Float/String to Float; invalid input raises ValueError." },
  get: { zh: "安全读取 List 索引、Map 键或对象字段；缺失返回 null。", en: "Safely read a List index, Map key, or record field; missing values return null." },
  require: { zh: "读取必需 Map 键；键缺失产生 KeyError，存在且为 null 时返回 null。", en: "Read a required Map key; missing raises KeyError, while a present null stays null." },
  pick: { zh: "返回只保留指定键的新 Map，并保留存在的 null 字段。", en: "Return a new Map containing selected keys, preserving present null fields." },
  put: { zh: "返回新增或替换一个键的新 Map，不修改原 Map。", en: "Return a new Map with one key inserted or replaced; the original is unchanged." },
  remove_key: { zh: "返回移除指定键的新 Map。", en: "Return a new Map without the named key." },
  append: { zh: "返回末尾增加一个元素的新 List。", en: "Return a new List with one item appended." },
  remove_at: { zh: "返回移除指定索引的新 List；越界产生 IndexError。", en: "Return a new List without the indexed item; out of range raises IndexError." },
  now: { zh: "返回带时区的当前 DateTime。", en: "Return the current zoned DateTime." },
  "datetime.parse": { zh: "按明确的格式和时区解析 DateTime，非法输入产生 ValueError。", en: "Parse DateTime using an explicit format and timezone; invalid input raises ValueError." },
  require_env: { zh: "读取必需环境变量；不存在时产生 KeyError。", en: "Read a required environment variable; missing raises KeyError." },
  sleep: { zh: "可取消地等待指定 Duration。", en: "Wait for a Duration while remaining cancellable." },
  cancel: { zh: "触发当前执行的根取消令牌并开始统一清理。", en: "Trigger the execution's root cancellation token and begin cleanup." },
  throw: { zh: "抛出 Error，并沿调用栈或 Flow 传播。", en: "Throw an Error through the call stack or Flow." },
  stream: { zh: "把 List、Map entries 或 Range 转成惰性单次消费 Stream。", en: "Convert a List, Map entries, or Range into a lazy single-use Stream." },
  range: { zh: "创建从 start 到 end（不含 end）的 Int Stream。", en: "Create an Int Stream from start up to but excluding end." },
  map: { zh: "惰性地对每项调用闭包，一项输入对应一项输出，不自动展开。", en: "Lazily transform each item one-to-one without automatic flattening." },
  flat_map: { zh: "对每项返回一个子 Stream，并惰性地把子流依次展开。", en: "Return a child Stream per item and lazily concatenate child streams." },
  where: { zh: "惰性保留闭包返回 true 的项目；闭包必须返回 Bool。", en: "Lazily retain items whose predicate returns Bool true." },
  take: { zh: "惰性保留前 n 项，达到数量后提前关闭上游。", en: "Lazily retain the first n items and close upstream early." },
  skip: { zh: "惰性丢弃前 n 项，然后传递其余项目。", en: "Lazily discard the first n items and pass the remainder." },
  inspect: { zh: "为每项执行观察闭包，再原样传递项目。", en: "Run an observation closure for each item and pass the item unchanged." },
  distinct: { zh: "惰性去除重复的可 Hash 标量，并保存已见集合。", en: "Lazily remove duplicate hashable scalars while retaining a seen set." },
  sort_by: { zh: "物化有限输入，按闭包 key 和 asc/desc 选项稳定排序。", en: "Materialize finite input and stably sort by closure key and asc/desc option." },
  group_by: { zh: "物化有限输入并按 Hash key 输出 Group；Group 含 key 与 values。", en: "Materialize finite input into Groups containing key and values." },
  debounce: { zh: "在指定 Duration 内合并快速连续事件，常用于 watch。", en: "Coalesce rapid events within a Duration, commonly for watch streams." },
  on_error: { zh: "当 Stream 失败时调用闭包，用返回的 Stream 恢复或替换后续输出。", en: "On Stream failure, invoke a closure whose returned Stream supplies recovery output." },
  parallel: { zh: "用最多 n 个隔离 worker 并发处理，保序、有界缓冲且 fail-fast。", en: "Process with at most n isolated workers, ordered output, bounded buffering, and fail-fast errors." },
  collect: { zh: "消费有限 Stream 并物化为 List。", en: "Consume a finite Stream and materialize it as a List." },
  count: { zh: "消费 Stream 并返回项目数。", en: "Consume a Stream and return its item count." },
  first: { zh: "返回第一项或 null，并提前关闭上游。", en: "Return the first item or null and close upstream early." },
  last: { zh: "消费 Stream 并返回最后一项或 null。", en: "Consume a Stream and return its last item or null." },
  min: { zh: "消费数值 Stream，返回最小值或空流的 null。", en: "Consume a numeric Stream and return its minimum or null for empty input." },
  max: { zh: "消费数值 Stream，返回最大值或空流的 null。", en: "Consume a numeric Stream and return its maximum or null for empty input." },
  sum: { zh: "消费数值 Stream 并求和，遵守 Int 溢出规则。", en: "Consume and sum a numeric Stream, respecting Int overflow rules." },
  reduce: { zh: "以 initial 累积 Stream；闭包接收含 acc/item/index 的 state。", en: "Fold a Stream from initial; the closure receives state with acc/item/index." },
  any: { zh: "任一项目满足谓词即返回 true，并短路关闭上游。", en: "Return true on the first matching item and short-circuit upstream." },
  all: { zh: "所有项目满足谓词才返回 true；首个 false 时短路。", en: "Return true only if every item matches; short-circuit on the first false." },
  for_each: { zh: "消费 Stream 并为每项执行闭包，返回 null。", en: "Consume a Stream, execute a closure for each item, and return null." },
  contains: { zh: "判断 String 是否含子串，或 List 是否含相等值。", en: "Test whether a String contains a substring or a List contains an equal value." },
  upper: { zh: "返回 Unicode 大写转换后的新 String。", en: "Return a new String converted to Unicode uppercase." },
  lower: { zh: "返回 Unicode 小写转换后的新 String。", en: "Return a new String converted to Unicode lowercase." },
  trim: { zh: "移除 String 两端空白。", en: "Remove whitespace from both ends of a String." },
  trim_start: { zh: "移除 String 开头空白。", en: "Remove leading whitespace from a String." },
  trim_end: { zh: "移除 String 末尾空白。", en: "Remove trailing whitespace from a String." },
  starts_with: { zh: "判断 String 是否以指定文本开头。", en: "Test whether a String starts with the given text." },
  ends_with: { zh: "判断 String 是否以指定文本结尾。", en: "Test whether a String ends with the given text." },
  replace: { zh: "返回把匹配文本替换后的新 String。", en: "Return a new String with matching text replaced." },
  split: { zh: "按分隔文本把 String 分割成 List<String>。", en: "Split a String by delimiter text into List<String>." },
  join: { zh: "用分隔文本连接 List<String>。", en: "Join List<String> with delimiter text." },
  regex_match: { zh: "判断 PCRE2 Regex 是否匹配 String，受正则资源限制。", en: "Test a String against a PCRE2 Regex under regex resource limits." },
  regex_captures: { zh: "返回完整匹配、字节位置、编号和命名捕获；不匹配返回 null。", en: "Return full match, byte positions, numbered and named captures; null when unmatched." },
  parse_json: { zh: "严格解析 JSON String 为普通 HHY 值，错误包含行列。", en: "Strictly parse JSON String into ordinary HHY values with line/column errors." },
  encode_json: { zh: "把可编码普通值转成 JSON；options 可启用 pretty。", en: "Encode supported ordinary values as JSON; options may enable pretty output." },
  parse_csv: { zh: "把 String 或行 Stream 流式解析成 Stream<Map>。", en: "Stream-parse a String or line Stream into Stream<Map>." },
  encode_csv: { zh: "把 Stream<Map> 流式编码为不含换行符的 CSV record Stream。", en: "Stream-encode Stream<Map> into CSV records without line terminators." },
  path: { zh: "把 String 词法规范化为 Path，不访问文件系统。", en: "Lexically normalize String into Path without filesystem access." },
  path_join: { zh: "组合 Path 与子路径并返回规范化的新 Path。", en: "Combine a Path with a child path and return a normalized Path." },
  files: { zh: "按 glob 惰性遍历根目录，返回 File/Directory Stream。", en: "Lazily walk a root with a glob and return a File/Directory Stream." },
  read_text: { zh: "完整读取 UTF-8 文件为 String。", en: "Read an entire UTF-8 file as String." },
  read_lines: { zh: "逐行惰性读取 UTF-8 文件并移除行终止符。", en: "Lazily read UTF-8 lines with terminators removed." },
  read_bytes: { zh: "完整读取二进制文件为 BytesBuffer。", en: "Read an entire binary file as BytesBuffer." },
  write_text: { zh: "以原子替换方式写 String，支持 overwrite/create_parents。", en: "Atomically replace with String, supporting overwrite/create_parents." },
  append_text: { zh: "把 String 追加到文件末尾。", en: "Append String to the end of a file." },
  write_bytes: { zh: "以原子替换方式写 BytesBuffer。", en: "Atomically replace a file with BytesBuffer." },
  save_text: { zh: "把 String 或文本 Stream 边拉取边原子保存。", en: "Atomically save a String or pull a text Stream directly to disk." },
  save_lines: { zh: "把 String Stream 逐项写入并补 LF，最终原子替换。", en: "Write a String Stream with LF per item and atomically replace the target." },
  copy: { zh: "复制文件，支持原子 no-replace 与创建父目录。", en: "Copy a file with atomic no-replace and parent creation options." },
  move: { zh: "移动或重命名文件，遵守覆盖选项。", en: "Move or rename a file while respecting overwrite options." },
  remove: { zh: "删除明确 Path，并返回该 Path。", en: "Remove an explicit Path and return it." },
  watch: { zh: "返回无限 FileEvent Stream，支持 recursive 选项并响应取消。", en: "Return an infinite FileEvent Stream with recursive option and cancellation." },
  run: { zh: "直接执行 argv，不经过 Shell；返回 CommandResult。", en: "Execute argv directly without a shell and return CommandResult." },
  shell: { zh: "显式用 Shell 执行 String；仅在需要重定向、管道等 Shell 语义时使用。", en: "Explicitly execute a String through a shell for redirects, pipes, and shell syntax." },
  stdout_lines: { zh: "把 CommandResult.stdout 转为惰性行 Stream。", en: "Expose CommandResult.stdout as a lazy line Stream." },
  processes: { zh: "获取当前进程快照的 Stream<Process>。", en: "Return a Stream<Process> snapshot of current processes." },
  stdin_lines: { zh: "惰性读取标准输入行，直到 EOF 或取消。", en: "Lazily read stdin lines until EOF or cancellation." },
  every: { zh: "按指定 Duration 产生无限计时 tick Stream。", en: "Produce an infinite timer tick Stream at a Duration interval." },
  "http.get": { zh: "构造 GET HttpRequest 计划，不发送网络请求。", en: "Build a GET HttpRequest plan without network I/O." },
  "http.post": { zh: "构造 POST HttpRequest 计划，不发送网络请求。", en: "Build a POST HttpRequest plan without network I/O." },
  "http.put": { zh: "构造 PUT HttpRequest 计划，不发送网络请求。", en: "Build a PUT HttpRequest plan without network I/O." },
  "http.delete": { zh: "构造 DELETE HttpRequest 计划，不发送网络请求。", en: "Build a DELETE HttpRequest plan without network I/O." },
  timeout: { zh: "返回设置请求超时的新 HttpRequest。", en: "Return a new HttpRequest with its timeout configured." },
  retry: { zh: "返回配置重试次数和退避的新 HttpRequest。", en: "Return a new HttpRequest configured with retry count and backoff." },
  send: { zh: "执行 HttpRequest 网络副作用并返回 HttpResponse。", en: "Perform the HttpRequest network effect and return HttpResponse." },
  response_body: { zh: "验证响应状态并把有界 body 解码为 UTF-8 String。", en: "Validate response status and decode the bounded body as UTF-8 String." },
  response_bytes: { zh: "验证响应状态并返回有界二进制 BytesBuffer。", en: "Validate response status and return the bounded binary BytesBuffer." }
};

function callableList(block: "stdCore" | "stdFlow" | "stdText" | "stdFiles" | "stdProcess" | "stdHttp", language: Language): DocBlock {
  return {
    type: "api",
    entries: code[block].split("\n").map((signature) => {
      const name = signature.slice(0, signature.indexOf("("));
      return { name, signature, description: callableDescriptions[name][language] };
    })
  };
}

function callableSelection(names: string[], language: Language): DocBlock {
  const signatures = [code.stdCore, code.stdFlow, code.stdText, code.stdFiles, code.stdProcess, code.stdHttp]
    .flatMap((block) => block.split("\n"));
  return {
    type: "api",
    entries: names.map((name) => {
      const signature = signatures.find((item) => item.startsWith(`${name}(`));
      if (!signature) throw new Error(`missing documented callable ${name}`);
      return { name, signature, description: callableDescriptions[name][language] };
    })
  };
}

export const chapters: Chapter[] = [
  {
    slug: "quick-start",
    order: 1,
    title: { zh: "快速开始", en: "Quick Start" },
    summary: { zh: "安装 HHY，构建解释器，并运行第一个脚本。", en: "Install HHY, build the interpreter, and run your first script." },
    sections: {
      zh: [
        { title: "方式一：直接下载 Release（推荐）", blocks: [
          { type: "p", text: `不需要修改 HHY Runtime 时，直接使用官方 ${hhyVersionLabel} 发行包最快。根据系统和 CPU 选择 darwin-arm64、linux-x86_64 或 linux-arm64；压缩包已包含 HHY 可执行文件、官方示例与数据库扩展、所需的非系统运行库、文档、许可证和构建信息。` },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/releases", label: "打开 HHY GitHub Releases ↗", description: "下载最新稳定版本、对应的 .sha256 文件或汇总 SHA256SUMS。" },
          { type: "code", language: "sh", code: `tar -xzf hhy-${hhyVersion}-PLATFORM-ARCH.tar.gz\ncd hhy-${hhyVersion}-PLATFORM-ARCH\n./bin/hhy --version\n./bin/hhy run examples/07-language-basics.hhy` },
          { type: "note", text: "保持 bin/ 与 lib/ 的相对位置不变，否则便携包可能找不到随包运行库。PLATFORM-ARCH 替换为 darwin-arm64、linux-x86_64 或 linux-arm64。" }
        ] },
        { title: "下载后校验与加入 PATH", blocks: [
          { type: "p", text: "运行下载内容前，应使用同名 .sha256 或 SHA256SUMS 验证文件完整性。macOS 自带 shasum，Linux 通常使用 sha256sum。" },
          { type: "code", language: "sh", code: `# macOS\nshasum -a 256 -c hhy-${hhyVersion}-darwin-arm64.tar.gz.sha256\n\n# Linux\nsha256sum -c hhy-${hhyVersion}-linux-x86_64.tar.gz.sha256\n\n# 当前终端加入 PATH（替换成实际绝对路径）\nexport PATH=\"/absolute/path/hhy-${hhyVersion}-PLATFORM-ARCH/bin:$PATH\"\nhhy --version` },
          { type: "p", text: "长期使用时，把 export PATH 行放进 shell 配置文件；或者继续通过发行目录中的 ./bin/hhy 运行，不需要系统级安装。" }
        ] },
        { title: "方式二：从源码构建", blocks: [
          { type: "p", text: `需要开发 Runtime、验证最新源码或自定义安装位置时再选择源码构建。HHY ${hhyVersionLabel} 正式支持 macOS arm64、Linux arm64 和 Linux x86_64；需要 C11 编译器、make、libcurl、PCRE2 与 BDWGC。数据库扩展还需要对应的 PostgreSQL libpq 或 MySQL client 开发库。` },
          { type: "code", language: "sh", code: "brew install curl pcre2 bdw-gc\ngit clone https://github.com/hh696-wq/hhy-vm.git\ncd hhy-vm\nmake\nmake test\n./build/hhy --version" },
          { type: "note", text: "brew 命令只适用于 macOS。Linux 的依赖包名称因发行版而异，完整说明见仓库 INSTALL.md。" }
        ] },
        { title: "安装源码构建结果", blocks: [
          { type: "code", language: "sh", code: "make install PREFIX=\"$(brew --prefix)\"\nhhy --version" },
          { type: "p", text: "PREFIX 可以换成自定义绝对路径。确认 PREFIX/bin 已在 PATH 后，所有 .hhy 文件都可以通过 hhy run 执行。" }
        ] },
        { title: "第一个脚本", blocks: [
          { type: "code", language: "hhy", code: code.hello, filename: "hello.hhy" },
          { type: "code", language: "sh", code: "hhy check hello.hhy\nhhy run hello.hhy" },
          { type: "p", text: "let 创建绑定；List 字面量保存三个 String；|> 把左侧值注入下一个函数；map 的闭包逐项生成新 String；print 消费结果。check 先验证词法、语法、作用域、模块与已知标准库调用，不执行副作用。" }
        ] },
        { title: "脚本运行与开发流程", blocks: [
          { type: "table", columns: ["任务", "命令", "用途"], rows: [["格式化", "hhy fmt script.hhy", "写入 HHY 官方格式"], ["检查格式", "hhy fmt --check script.hhy", "在 CI 中检查，不修改文件"], ["检查脚本", "hhy check script.hhy", "检查语法、作用域和已知 API"], ["运行脚本", "hhy run script.hhy", "执行脚本"], ["传递参数", "hhy run script.hhy input.csv output.json", "参数进入只读 args"], ["预览计划", "hhy run --dry-run script.hhy", "查看脱敏计划，不执行外部副作用"]] },
          { type: "p", text: "源码使用 .hhy 后缀。查看完整命令：" },
          { type: "code", language: "sh", code: "hhy --help" }
        ] }
      ],
      en: [
        { title: "Option 1: download a Release (recommended)", blocks: [
          { type: "p", text: `If you are not modifying the HHY Runtime, an official ${hhyVersionLabel} archive is the fastest path. Choose darwin-arm64, linux-x86_64, or linux-arm64 for your OS and CPU. Archives include the executable, official sample and database extensions, required non-system runtime libraries, docs, licenses, and build metadata.` },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/releases", label: "Open HHY GitHub Releases ↗", description: "Download the latest stable archive and its matching .sha256 file or SHA256SUMS." },
          { type: "code", language: "sh", code: `tar -xzf hhy-${hhyVersion}-PLATFORM-ARCH.tar.gz\ncd hhy-${hhyVersion}-PLATFORM-ARCH\n./bin/hhy --version\n./bin/hhy run examples/07-language-basics.hhy` },
          { type: "note", text: "Keep bin/ and lib/ in their original relative positions so the portable executable can find bundled libraries. Replace PLATFORM-ARCH with darwin-arm64, linux-x86_64, or linux-arm64." }
        ] },
        { title: "Verify the download and add it to PATH", blocks: [
          { type: "p", text: "Before running a download, verify it with the matching .sha256 file or SHA256SUMS. macOS includes shasum; Linux commonly provides sha256sum." },
          { type: "code", language: "sh", code: `# macOS\nshasum -a 256 -c hhy-${hhyVersion}-darwin-arm64.tar.gz.sha256\n\n# Linux\nsha256sum -c hhy-${hhyVersion}-linux-x86_64.tar.gz.sha256\n\n# Add to PATH for this terminal (use the real absolute path)\nexport PATH=\"/absolute/path/hhy-${hhyVersion}-PLATFORM-ARCH/bin:$PATH\"\nhhy --version` },
          { type: "p", text: "For permanent access, put the export PATH line in your shell profile. You may also keep invoking ./bin/hhy from the extracted directory without a system-wide install." }
        ] },
        { title: "Option 2: build from source", blocks: [
          { type: "p", text: `Build from source when developing the Runtime, validating current source, or choosing a custom installation prefix. HHY ${hhyVersionLabel} supports macOS arm64, Linux arm64, and Linux x86_64 and requires a C11 compiler, make, libcurl, PCRE2, and BDWGC. The database extension additionally needs the corresponding PostgreSQL libpq or MySQL client development library.` },
          { type: "code", language: "sh", code: "brew install curl pcre2 bdw-gc\ngit clone https://github.com/hh696-wq/hhy-vm.git\ncd hhy-vm\nmake\nmake test\n./build/hhy --version" },
          { type: "note", text: "The brew command applies only to macOS. Linux package names vary by distribution; see INSTALL.md for the full dependency matrix." }
        ] },
        { title: "Install the source build", blocks: [
          { type: "code", language: "sh", code: "make install PREFIX=\"$(brew --prefix)\"\nhhy --version" },
          { type: "p", text: "PREFIX may be a custom absolute path. Once PREFIX/bin is on PATH, execute any .hhy file with hhy run." }
        ] },
        { title: "Your first script", blocks: [
          { type: "code", language: "hhy", code: code.hello, filename: "hello.hhy" },
          { type: "code", language: "sh", code: "hhy check hello.hhy\nhhy run hello.hhy" },
          { type: "p", text: "let creates a binding; the List literal stores three Strings; |> injects the left value into the next call; the map closure creates one new String per item; print consumes the result. check validates lexical syntax, scope, modules, and known standard-library calls without effects." }
        ] },
        { title: "Running scripts and the development workflow", blocks: [
          { type: "table", columns: ["Task", "Command", "Purpose"], rows: [["Format", "hhy fmt script.hhy", "Write canonical HHY formatting"], ["Check format", "hhy fmt --check script.hhy", "Verify in CI without changing files"], ["Check script", "hhy check script.hhy", "Validate syntax, scope, and known APIs"], ["Run", "hhy run script.hhy", "Execute the script"], ["Pass arguments", "hhy run script.hhy input.csv output.json", "Arguments enter read-only args"], ["Preview plan", "hhy run --dry-run script.hhy", "Inspect a redacted plan without external effects"]] },
          { type: "p", text: "HHY source files use the .hhy suffix. View complete command help:" },
          { type: "code", language: "sh", code: "hhy --help" }
        ] }
      ]
    }
  },
  {
    slug: "language-basics",
    order: 2,
    title: { zh: "语言基础", en: "Language Basics" },
    summary: { zh: "变量、值、函数、条件、循环和作用域。", en: "Variables, values, functions, conditions, loops, and scope." },
    sections: {
      zh: [
        { title: "动态类型是什么意思", blocks: [
          { type: "p", text: "HHY 的变量声明不写类型，值在运行时携带自己的逻辑类型。动态类型不等于随意转换：条件必须得到 Bool，String 不会自动变成 Number、Bool 或 Path，参数数量和不支持的运算都会产生结构化错误。用 type(value) 查看类型，用 is_type(value, name) 判断类型。" },
          { type: "code", language: "hhy", code: code.basicsValues }
        ] },
        { title: "标量与单位类型", blocks: [
          { type: "table", columns: ["类型", "示例", "用途"], rows: [["Null", "null", "表示没有值"], ["Bool", "true", "条件与谓词"], ["Int", "42", "整数计算"], ["Float", "3.14", "浮点计算"], ["String", "\"hello\"", "UTF-8 文本"], ["Regex", "/ERROR/i", "文本匹配"], ["Bytes", "10mib", "文件或内存大小"], ["Duration", "5s", "超时与时间间隔"], ["Percent", "80%", "比例"], ["DateTime", "now()", "带时区时间"], ["Path", "path(\"logs\")", "文件系统路径"]] },
          { type: "p", text: "String、数字、单位和 Path 的精确边界行为属于 Reference。日常脚本只需记住：HHY 不会在 String、Number、Bool 和 Path 之间做隐式转换。" },
          { type: "link", href: "/zh/learn/syntax-reference", label: "查看类型与语法参考 →", description: "查阅 UTF-8、数值溢出、运算符和字面量的精确定义。" }
        ] },
        { title: "List、Map 与 Range", blocks: [
          { type: "p", text: "List 使用从 0 开始的索引，越界产生 IndexError。Map 的键只能是 String，保持插入顺序；map.key 与 map[\"key\"] 等价。普通缺失键返回 null，require 用于区分“键缺失”和“键存在但值是 null”。Range a..b 包含 a、不包含 b，并且不会预先分配 List。" },
          { type: "code", language: "hhy", code: code.basicsCollections },
          { type: "p", text: "List 和 Map 不原地修改。append、remove_at、put、remove_key、pick 都返回新集合，所以示例中的 original 和 config 保持不变。List/Map 支持深度相等；Function、Stream 和系统资源对象不支持值相等。" }
        ] },
        { title: "Result、Stream 与系统对象", blocks: [
          { type: "table", columns: ["类型", "用于"], rows: [["Result", "显式保存一次操作的成功值或 Error"], ["Stream", "惰性处理文件、行、进程、响应和事件"], ["Error", "携带类别、位置和 Flow stage 的失败"], ["Function", "用户函数与闭包"], ["系统对象", "File、Process、HttpResponse 等带只读字段的专用值"]] },
          { type: "p", text: "系统对象不是 Map。需要写入 JSON 时，先用 map 或 pick 选择普通字段。Stream 的惰性和消费规则在 Flow 章节展开。" }
        ] },
        { title: "变量、作用域与不可变性", blocks: [
          { type: "code", language: "hhy", code: "let service = \"api\"\nlet mut retries = 0\nretries = retries + 1" },
          { type: "p", text: "let 创建不可重新赋值的绑定；需要重新赋值时使用 let mut。List 和 Map 的更新函数返回新值，不修改原集合。变量遵循块级词法作用域，并且必须先声明后使用。" },
          { type: "note", text: "闭包可以捕获外层值。捕获 let mut 的闭包不能发送到 parallel worker；并发限制在“并发与监听”章节说明。" }
        ] },
        { title: "条件、循环与函数", blocks: [
          { type: "code", language: "hhy", code: code.basicsControl },
          { type: "p", text: "支持 if / else if / else、for item in iterable、while、break 和 continue。for 可以遍历 List、Map entries、Range 或 Stream；遍历 Stream 会消费它。函数使用位置参数，参数数量在调用时检查；没有显式 return 时返回 null。" },
          { type: "code", language: "hhy", code: code.basics },
          { type: "p", text: `闭包写作 { item -> expression }；多条语句时必须显式写参数并用 return 返回。单参数闭包在明确的 Flow 上下文中可以使用 { it * 2 }。${hhyVersionLabel} 不支持重载、泛型或默认参数。` }
        ] }
      ],
      en: [
        { title: "What dynamic typing means", blocks: [
          { type: "p", text: "HHY declarations omit types; each value carries its logical type at runtime. Dynamic does not mean coercive: conditions require Bool, String never automatically becomes Number, Bool, or Path, and invalid arity or operations raise structured errors. Use type(value) to inspect a type and is_type(value, name) to test it." },
          { type: "code", language: "hhy", code: code.basicsValues }
        ] },
        { title: "Scalar and unit types", blocks: [
          { type: "table", columns: ["Type", "Example", "Use"], rows: [["Null", "null", "Absence"], ["Bool", "true", "Conditions and predicates"], ["Int", "42", "Integer arithmetic"], ["Float", "3.14", "Floating-point arithmetic"], ["String", "\"hello\"", "UTF-8 text"], ["Regex", "/ERROR/i", "Text matching"], ["Bytes", "10mib", "File or memory size"], ["Duration", "5s", "Timeouts and intervals"], ["Percent", "80%", "Ratios"], ["DateTime", "now()", "Zoned time"], ["Path", "path(\"logs\")", "Filesystem paths"]] },
          { type: "p", text: "Precise String, number, unit, and Path edge cases belong in Reference. For ordinary scripts, remember that HHY never implicitly converts among String, Number, Bool, and Path." },
          { type: "link", href: "/en/learn/syntax-reference", label: "Open the type and syntax reference →", description: "Look up exact UTF-8, overflow, operator, and literal behavior." }
        ] },
        { title: "List, Map, and Range", blocks: [
          { type: "p", text: "List indices start at zero and out-of-range access raises IndexError. Map keys are Strings and preserve insertion order; map.key equals map[\"key\"]. A missing key normally returns null, while require distinguishes a missing key from a present key whose value is null. Range a..b includes a and excludes b without allocating a List." },
          { type: "code", language: "hhy", code: code.basicsCollections },
          { type: "p", text: "Lists and Maps are not mutated in place. append, remove_at, put, remove_key, and pick return new collections, leaving original and config unchanged. Lists and Maps support deep equality; Functions, Streams, and system resources do not support value equality." }
        ] },
        { title: "Result, Stream, and system objects", blocks: [
          { type: "table", columns: ["Type", "Used for"], rows: [["Result", "Explicit success values or Errors from one operation"], ["Stream", "Lazy files, lines, processes, responses, and events"], ["Error", "Failures with category, location, and Flow stage"], ["Function", "User functions and closures"], ["System object", "Dedicated values such as File, Process, and HttpResponse"]] },
          { type: "p", text: "System objects are not Maps. Map or pick ordinary fields before JSON encoding. Flow explains Stream laziness and consumption in detail." }
        ] },
        { title: "Bindings, scope, and immutability", blocks: [
          { type: "code", language: "hhy", code: "let service = \"api\"\nlet mut retries = 0\nretries = retries + 1" },
          { type: "p", text: "let creates a binding that cannot be reassigned; use let mut when reassignment is required. List and Map update functions return new collections rather than mutating originals. Names follow block lexical scope and must be declared before use." },
          { type: "note", text: "Closures may capture outer values. A closure that captures let mut cannot be sent to a parallel worker; Parallel and Watch covers this concurrency boundary." }
        ] },
        { title: "Conditions, loops, and functions", blocks: [
          { type: "code", language: "hhy", code: code.basicsControl },
          { type: "p", text: "HHY supports if / else if / else, for item in iterable, while, break, and continue. for iterates Lists, Map entries, Ranges, or Streams; iterating a Stream consumes it. Functions use positional arguments checked at call time and return null without an explicit return." },
          { type: "code", language: "hhy", code: code.basics },
          { type: "p", text: `A closure is { item -> expression }; a multi-statement closure must name its parameter and use return. A one-argument closure in an unambiguous Flow context may use { it * 2 }. ${hhyVersionLabel} has no overloading, generics, or default arguments.` }
        ] }
      ]
    }
  },
  {
    slug: "flow-and-streams",
    order: 3,
    title: { zh: "Flow 与 Stream", en: "Flow and Streams" },
    summary: { zh: "理解管道传值、惰性流和单次消费语义。", en: "Understand pipe injection, lazy streams, and single-consumption semantics." },
    sections: {
      zh: [
        { title: "Pipe 如何传值", blocks: [
          { type: "p", text: "Pipe 是普通函数调用的组合规则：x |> f 等价于 f(x)，x |> f(a) 等价于 f(x, a)，x |> obj.f(a) 等价于 obj.f(x, a)。它不会自动把标量变成 Stream、展开嵌套 Stream、访问 it 字段、忽略错误、字符串化值或启动 Shell。" },
          { type: "code", language: "hhy", code: code.flow }
        ] },
        { title: "Stream 的生命周期", blocks: [
          { type: "p", text: "Stream 是惰性、拉取式、单次消费序列。创建管道只组合 operator；终端开始拉取时，上游才逐项产生数据。生命周期是 open → next* → close，正常结束、take 提前停止、错误和取消都会从下游向上游关闭资源。" },
          { type: "table", columns: ["阶段", "发生的事情"], rows: [["创建", "Source 返回 Stream，但尚未读取数据"], ["组合", "map、where 等 operator 连接成 Pipeline"], ["消费", "print、collect、save 等终端开始逐项拉取"], ["关闭", "完成、提前停止、Error 或取消释放上游资源"]] },
          { type: "note", text: "Stream 只能消费一次。不要把同一个 Stream 保存后交给两条 Pipeline；需要重复处理时重新创建 Source，或在有限输入上显式 collect。" }
        ] },
        { title: "逐项、过滤与观察算子", blocks: [
          { type: "code", language: "hhy", code: code.flowOperators },
          callableSelection(["map", "where", "take", "skip", "inspect", "distinct"], "zh")
        ] },
        { title: "map 与 flat_map 的区别", blocks: [
          { type: "p", text: "map 的闭包返回什么，下游就收到什么。如果返回 Stream，结果是 Stream<Stream<T>>。flat_map 要求闭包返回 Stream，并把每个子流依次展开成一条 Stream。" },
          { type: "code", language: "hhy", code: code.flowFlatMap }
        ] },
        { title: "Barrier 和终端算子到底做什么", blocks: [
          { type: "p", text: "逐项算子只需保存当前项；Barrier 必须先看完或保存大量输入才能产生正确结果。sort_by 要保存全部输入后排序，group_by 要保存每组的全部 values，collect 把全部项组成 List，reduce/count/sum 等终端算子读取到结束才返回标量。它们都受 max_memory、集合大小和运行时间限制。" },
          { type: "code", language: "hhy", code: code.flowBarriers },
          callableSelection(["sort_by", "group_by", "collect", "reduce", "count", "sum", "min", "max", "first", "last", "any", "all"], "zh"),
          { type: "note", text: "不要把 watch、every 或没有明确上限的输入直接送入 sort_by、group_by 或 collect。先用 take、时间窗口或其他业务边界把输入限制为有限流，否则 Runtime 会产生 PlanError。" }
        ] },
        { title: "副作用、错误与并发", blocks: [
          { type: "p", text: "只有终端或 Action 才会真正消费 Pipeline。print、for_each、save_*、run 和 send 会执行输出、文件、进程或网络操作。普通 Error 默认终止 Pipeline；需要逐项保留失败时使用 attempt，需要替换整条失败上游时使用 on_error。" },
          { type: "p", text: "parallel(n) 使用隔离 worker 并保持输出顺序。并发数量、可发送值和取消语义在“并发与监听”章节展开。" }
        ] }
      ],
      en: [
        { title: "How Pipe passes values", blocks: [
          { type: "p", text: "Pipe is a composition rule for ordinary calls: x |> f means f(x), x |> f(a) means f(x, a), and x |> obj.f(a) means obj.f(x, a). It does not turn scalars into Streams, flatten nested Streams, access it fields, ignore errors, stringify values, or invoke a shell." },
          { type: "code", language: "hhy", code: code.flow }
        ] },
        { title: "Stream lifecycle", blocks: [
          { type: "p", text: "A Stream is a lazy, pull-based, single-consumption sequence. Building a pipeline only composes operators; upstream produces items when a terminal starts pulling. Its lifecycle is open → next* → close, and normal completion, early take, errors, and cancellation all close resources upstream." },
          { type: "table", columns: ["Stage", "What happens"], rows: [["Create", "A Source returns a Stream without reading data"], ["Compose", "Operators such as map and where form a Pipeline"], ["Consume", "A terminal such as print, collect, or save starts pulling"], ["Close", "Completion, early stop, Error, or cancellation releases upstream resources"]] },
          { type: "note", text: "A Stream is consumed once. Do not save one Stream and feed two Pipelines; recreate the Source, or explicitly collect finite input." }
        ] },
        { title: "Item, filter, and observation operators", blocks: [
          { type: "code", language: "hhy", code: code.flowOperators },
          callableSelection(["map", "where", "take", "skip", "inspect", "distinct"], "en")
        ] },
        { title: "map versus flat_map", blocks: [
          { type: "p", text: "map sends exactly the closure result downstream. Returning a Stream therefore creates Stream<Stream<T>>. flat_map requires a Stream result and concatenates each child stream into one Stream." },
          { type: "code", language: "hhy", code: code.flowFlatMap }
        ] },
        { title: "What barriers and terminals actually do", blocks: [
          { type: "p", text: "Item operators retain only the current item. A barrier must inspect or retain substantial input before producing a correct result: sort_by stores all input before sorting, group_by stores every group's values, collect builds a List, and terminals such as reduce/count/sum read to completion before returning a scalar. All obey memory, collection-size, and runtime limits." },
          { type: "code", language: "hhy", code: code.flowBarriers },
          callableSelection(["sort_by", "group_by", "collect", "reduce", "count", "sum", "min", "max", "first", "last", "any", "all"], "en"),
          { type: "note", text: "Do not feed watch, every, or otherwise unbounded input directly into sort_by, group_by, or collect. Apply take, a time window, or another business bound first, or the Runtime raises PlanError." }
        ] },
        { title: "Effects, errors, and parallelism", blocks: [
          { type: "p", text: "Only a terminal or Action consumes a Pipeline. print, for_each, save_*, run, and send perform output, filesystem, process, or network work. Ordinary Errors terminate the Pipeline; use attempt for per-item Results and on_error to replace a failed upstream Stream." },
          { type: "p", text: "parallel(n) uses isolated workers and preserves output order. Parallel and Watch covers concurrency limits, Sendable values, and cancellation." }
        ] }
      ]
    }
  },
  {
    slug: "files-and-paths",
    order: 4,
    title: { zh: "文件与路径", en: "Files and Paths" },
    summary: { zh: "遍历目录、读取文本并安全写入结果。", en: "Walk directories, read text, and write results safely." },
    sections: {
      zh: [
        { title: "Path 不是 String", blocks: [
          { type: "p", text: "所有文件 API 都要求 Path。path(text) 做词法规范化：折叠重复分隔符和 .，消除可以解析的 ..，但不访问文件系统也不解析符号链接。相对 Path 始终基于进程启动目录，不随被 import 的文件位置变化。" },
          { type: "code", language: "hhy", code: code.pathFields },
          { type: "p", text: "name、extension、parent 是 Path 的只读字段，不是 path_name()、path_extension()、path_parent() 函数。extension 包含前导点，无扩展名时为空字符串；path_join(base, child) 返回组合后的新 Path。" }
        ] },
        { title: "files：遍历、glob 与元数据", blocks: [
          { type: "code", language: "hhy", code: code.files },
          { type: "p", text: "files(root, pattern, options?) 返回惰性的 Stream<File | Directory>，不会返回遍历根本身。pattern 支持 *、?、**；默认不跟随目录符号链接，{ follow_symlinks: true } 可开启并自动跳过检测到的目录循环。" },
          { type: "table", columns: ["字段", "含义"], rows: [["path", "完整 Path"], ["name", "文件或目录名"], ["extension", "包含前导点的扩展名"], ["size", "Bytes 大小"], ["created", "创建时间；不可可靠取得时为 null"], ["modified", "修改时间"], ["is_file / is_dir / is_symlink", "对象种类标记"]] },
          { type: "p", text: "File 和 Directory 是系统对象，不是 Map。写入 JSON 前，先把需要的字段映射成普通 Map。" }
        ] },
        { title: "读取文本与二进制", blocks: [
          callableSelection(["read_text", "read_lines", "read_bytes"], "zh"),
          { type: "note", text: "文本 API 验证 UTF-8。图片、压缩包等任意二进制使用 read_bytes 和 write_bytes，不要放进 String。" }
        ] },
        { title: "写入、追加与原子保存", blocks: [
          { type: "code", language: "hhy", code: code.fileOperations },
          callableSelection(["write_text", "append_text", "write_bytes", "save_text", "save_lines"], "zh"),
          { type: "p", text: "write_text、write_bytes、save_text、save_lines 的 options 支持 overwrite（默认 true）和 create_parents（默认 false）。这些 API 通过同目录临时文件加 rename 提交；overwrite: false 使用原子 no-replace，避免先检查后写入的竞态覆盖。" }
        ] },
        { title: "复制、移动、删除与 dry-run", blocks: [
          callableSelection(["copy", "move", "remove"], "zh"),
          { type: "code", language: "sh", code: "hhy run --dry-run backup.hhy\nhhy run backup.hhy" },
          { type: "p", text: "先检查 dry-run 计划，再执行包含复制、移动或删除的脚本。" },
          { type: "note", text: "文件读取、遍历、写入、进程和网络操作都可能被 RuntimeLimits、取消或宿主权限中止。不要依赖 GC 关闭系统资源；Runtime 会在正常完成、错误、return、exit 和 cancel 路径显式清理。" }
        ] },
        { title: "查阅完整 API", blocks: [
          { type: "link", href: "/zh/learn/standard-library#fn-path", label: "路径与文件 API Reference →", description: "查阅全部签名、参数形式和函数锚点。" }
        ] }
      ],
      en: [
        { title: "Path is not String", blocks: [
          { type: "p", text: "Every filesystem API requires Path. path(text) normalizes lexically: it collapses repeated separators and ., and resolves removable .. segments without accessing the filesystem or resolving symlinks. A relative Path is always based on the process startup directory, not the importing file." },
          { type: "code", language: "hhy", code: code.pathFields },
          { type: "p", text: "name, extension, and parent are read-only Path fields—not path_name(), path_extension(), or path_parent() functions. extension includes the leading dot and is empty when absent; path_join(base, child) returns a new combined Path." }
        ] },
        { title: "files: traversal, globs, and metadata", blocks: [
          { type: "code", language: "hhy", code: code.files },
          { type: "p", text: "files(root, pattern, options?) returns a lazy Stream<File | Directory> and excludes the traversal root itself. Patterns support *, ?, and **. Directory symlinks are not followed by default; { follow_symlinks: true } opts in with cycle detection." },
          { type: "table", columns: ["Field", "Meaning"], rows: [["path", "Full Path"], ["name", "File or directory name"], ["extension", "Extension including its leading dot"], ["size", "Size as Bytes"], ["created", "Creation time, or null when unavailable"], ["modified", "Modification time"], ["is_file / is_dir / is_symlink", "Object-kind flags"]] },
          { type: "p", text: "File and Directory are system objects, not Maps. Map required fields into an ordinary Map before JSON encoding." }
        ] },
        { title: "Reading text and binary data", blocks: [
          callableSelection(["read_text", "read_lines", "read_bytes"], "en"),
          { type: "note", text: "Text APIs validate UTF-8. Use read_bytes and write_bytes for images, archives, and arbitrary binary data rather than storing it in String." }
        ] },
        { title: "Writing, appending, and atomic saves", blocks: [
          { type: "code", language: "hhy", code: code.fileOperations },
          callableSelection(["write_text", "append_text", "write_bytes", "save_text", "save_lines"], "en"),
          { type: "p", text: "write_text, write_bytes, save_text, and save_lines accept overwrite (default true) and create_parents (default false). They commit through a same-directory temporary file plus rename; overwrite: false uses atomic no-replace to prevent check-then-write races." }
        ] },
        { title: "Copy, move, remove, and dry-run", blocks: [
          callableSelection(["copy", "move", "remove"], "en"),
          { type: "code", language: "sh", code: "hhy run --dry-run backup.hhy\nhhy run backup.hhy" },
          { type: "p", text: "Review the dry-run plan before executing a script that copies, moves, or removes files." },
          { type: "note", text: "Filesystem reads, traversal, writes, processes, and networking may stop because of RuntimeLimits, cancellation, or host permissions. The Runtime explicitly cleans resources on completion, errors, return, exit, and cancel; it does not rely on GC finalizers." }
        ] },
        { title: "Look up the complete API", blocks: [
          { type: "link", href: "/en/learn/standard-library#fn-path", label: "Paths and files API Reference →", description: "Look up every signature, parameter form, and stable function anchor." }
        ] }
      ]
    }
  },
  {
    slug: "text-json-csv",
    order: 5,
    title: { zh: "文本、JSON 与 CSV", en: "Text, JSON, and CSV" },
    summary: { zh: "处理 UTF-8 文本、正则表达式和结构化数据。", en: "Process UTF-8 text, regular expressions, and structured data." },
    sections: {
      zh: [
        { title: "String 与 UTF-8", blocks: [
          { type: "p", text: "String 是不可变 UTF-8 字节序列。length 统计 Unicode code point，byte_length 统计编码后的字节；索引返回一个 code point 对应的单字符 String。文本函数返回新值，不修改原 String。" },
          { type: "code", language: "hhy", code: "let line = \"  ERROR: timeout  \"\n\nline\n    |> trim\n    |> replace(\"ERROR\", \"WARN\")\n    |> lower\n    |> print" },
          callableSelection(["trim", "split", "join", "replace", "contains", "starts_with", "ends_with", "lower", "upper"], "zh")
        ] },
        { title: "Regex", blocks: [
          { type: "p", text: "Regex 字面量写作 /pattern/flags，支持 i（忽略大小写）、m（多行）、s（点匹配换行）和 u。regex_match 只返回是否匹配；regex_captures 返回完整匹配、字节位置、groups 编号捕获与 named 命名捕获，不匹配时返回 null。" },
          { type: "note", text: `${hhyVersionLabel} 使用 PCRE2 8-bit，并限制 pattern、subject、match、depth、heap 和捕获组数量；超限产生 ResourceLimitError，避免恶意正则耗尽运行时。` }
        ] },
        { title: "JSON 的类型映射与错误", blocks: [
          { type: "code", language: "hhy", code: code.json },
          { type: "table", columns: ["JSON", "HHY"], rows: [["object", "Map"], ["array", "List"], ["string", "String"], ["integer", "Int"], ["decimal", "Float"], ["true / false", "Bool"], ["null", "Null"]] },
          { type: "p", text: "parse_json 的错误包含行列。encode_json 可以使用 { pretty: true } 输出可读格式。Function、Stream 和系统对象不能直接编码；先选择普通字段。" }
        ] },
        { title: "CSV 是流式 record", blocks: [
          { type: "p", text: "parse_csv 接受完整 String 或 Stream<String>，返回 Stream<Map>；encode_csv 接受 Stream<Map>，返回不带行终止符的 Stream<String>。两者不需要加载完整文件。" },
          { type: "code", language: "hhy", code: "read_lines(path(\"employees.csv\"))\n    |> parse_csv({ header: true })\n    |> where { row -> row.active == \"true\" }\n    |> encode_csv({ header: true })\n    |> save_lines(path(\"active-employees.csv\"))" },
          { type: "p", text: "header 控制首行字段名，delimiter 和 quote 必须是单字符。CSV 不做 schema 推断；数字和 Bool 需要显式转换。encode_csv 不附加换行符，与 save_lines 配合写出。" }
        ] },
        { title: "查阅完整 API", blocks: [{ type: "link", href: "/zh/learn/standard-library#fn-contains", label: "文本与结构化数据 API Reference →", description: "查阅文本、Regex、JSON 和 CSV 的完整函数签名。" }] }
      ],
      en: [
        { title: "String and UTF-8", blocks: [
          { type: "p", text: "String is an immutable UTF-8 byte sequence. length counts Unicode code points, byte_length counts encoded bytes, and indexing returns a one-code-point String. Text functions return new values and never mutate the original." },
          { type: "code", language: "hhy", code: "let line = \"  ERROR: timeout  \"\n\nline\n    |> trim\n    |> replace(\"ERROR\", \"WARN\")\n    |> lower\n    |> print" },
          callableSelection(["trim", "split", "join", "replace", "contains", "starts_with", "ends_with", "lower", "upper"], "en")
        ] },
        { title: "Regex", blocks: [
          { type: "p", text: "A Regex literal is /pattern/flags with i (case-insensitive), m (multiline), s (dot matches newline), and u. regex_match returns Bool; regex_captures returns the full match, byte positions, numbered groups, and named captures, or null when unmatched." },
          { type: "note", text: `${hhyVersionLabel} uses PCRE2 8-bit with pattern, subject, match, depth, heap, and capture limits. Exceeding them raises ResourceLimitError.` }
        ] },
        { title: "JSON type mapping and errors", blocks: [
          { type: "code", language: "hhy", code: code.json },
          { type: "table", columns: ["JSON", "HHY"], rows: [["object", "Map"], ["array", "List"], ["string", "String"], ["integer", "Int"], ["decimal", "Float"], ["true / false", "Bool"], ["null", "Null"]] },
          { type: "p", text: "parse_json reports line and column on failure. encode_json accepts { pretty: true } for readable output. Function, Stream, and system objects require mapping to ordinary fields first." }
        ] },
        { title: "CSV is a record stream", blocks: [
          { type: "p", text: "parse_csv accepts a complete String or Stream<String> and returns Stream<Map>; encode_csv accepts Stream<Map> and returns Stream<String> records without terminators. Neither requires a whole-file buffer." },
          { type: "code", language: "hhy", code: "read_lines(path(\"employees.csv\"))\n    |> parse_csv({ header: true })\n    |> where { row -> row.active == \"true\" }\n    |> encode_csv({ header: true })\n    |> save_lines(path(\"active-employees.csv\"))" },
          { type: "p", text: "header controls field names; delimiter and quote must be single characters. CSV performs no schema inference, so numbers and Bools require explicit conversion. encode_csv omits terminators and composes with save_lines." }
        ] },
        { title: "Look up the complete API", blocks: [{ type: "link", href: "/en/learn/standard-library#fn-contains", label: "Text and structured data API Reference →", description: "Look up complete signatures for text, Regex, JSON, and CSV functions." }] }
      ]
    }
  },
  {
    slug: "processes",
    order: 6,
    title: { zh: "进程与系统", en: "Processes and System" },
    summary: { zh: "运行命令、消费输出并检查系统状态。", en: "Run commands, consume output, and inspect system state." },
    sections: {
      zh: [
        { title: "run 与 shell 的安全边界", blocks: [
          { type: "code", language: "hhy", code: code.process },
          { type: "p", text: "run(argv, options?) 直接把 List<String> 交给操作系统，不经过 Shell，因此空格、通配符、$、重定向和管道不会被二次解释。只有确实需要 Shell 语法时才使用 shell(command, options?)；Checker 会对 shell 给出安全提示。" },
          { type: "table", columns: ["选项", "用途"], rows: [["cwd", "子进程工作目录 Path"], ["env", "只覆盖子进程环境"], ["stdin", "传给命令的标准输入文本"], ["timeout", "命令最长运行时间"], ["max_output", "stdout 与 stderr 捕获上限"]] },
          { type: "note", text: "包含用户输入时优先使用 run。shell 会解释重定向、管道和变量展开，只在明确需要 Shell 语义时使用。" }
        ] },
        { title: "CommandResult", blocks: [
          { type: "p", text: "run 与 shell 默认等待结束并返回 CommandResult，而不是把非零退出码自动当作 HHY Error。脚本应读取 exit_code 决定业务成功。" },
          { type: "table", columns: ["字段", "内容"], rows: [["exit_code", "子进程退出码"], ["stdout", "标准输出 String"], ["stderr", "标准错误 String"], ["duration", "命令运行 Duration"]] },
          { type: "p", text: "非零 exit_code 不会自动变成 HHY Error。脚本需要根据命令约定判断成功。stdout_lines(result) 可把已捕获输出作为行 Stream 继续处理。" }
        ] },
        { title: "进程快照与字段", blocks: [
          { type: "p", text: "processes() 返回当前时刻的 Stream<Process> 快照。Process 不是 Map，提供 pid、name、cpu、memory、status、command 只读字段；转 JSON 前必须显式映射成普通 Map。" },
          { type: "note", text: "排序示例使用 sort_by({ order: \"desc\" })；order 只能是 asc 或 desc，默认 asc。排序是稳定 barrier，会物化有限快照。" }
        ] },
        { title: "args、env、system 与 stdin", blocks: [
          { type: "table", columns: ["值", "内容"], rows: [["args", "不含脚本路径的 List<String>"], ["env", "只读环境变量视图"], ["system", "OS、架构、主机、CPU、内存和目录信息"], ["stdin_lines()", "标准输入行 Stream"]] },
          { type: "code", language: "hhy", code: "if length(args) != 1 {\n    print_error(\"usage: script.hhy <input>\")\n    exit(3)\n}\n\nlet input = path(args[0])" }
        ] },
        { title: "查阅完整 API", blocks: [{ type: "link", href: "/zh/learn/standard-library#fn-run", label: "进程与系统 API Reference →", description: "查阅 run、shell、processes、stdin_lines 和 every 的完整签名。" }] }
      ],
      en: [
        { title: "The safety boundary between run and shell", blocks: [
          { type: "code", language: "hhy", code: code.process },
          { type: "p", text: "run(argv, options?) passes List<String> directly to the OS without a shell, so spaces, globs, $, redirects, and pipes are not reinterpreted. Use shell(command, options?) only when shell syntax is intentional; the Checker emits a safety hint." },
          { type: "table", columns: ["Option", "Purpose"], rows: [["cwd", "Child working-directory Path"], ["env", "Overrides only the child environment"], ["stdin", "Text supplied on standard input"], ["timeout", "Maximum command duration"], ["max_output", "stdout and stderr capture limit"]] },
          { type: "note", text: "Prefer run when values include user input. shell interprets redirects, pipes, and expansion and should be reserved for intentional shell syntax." }
        ] },
        { title: "CommandResult", blocks: [
          { type: "p", text: "run and shell wait by default and return CommandResult. A nonzero child exit code is not automatically an HHY Error; inspect exit_code for business success." },
          { type: "table", columns: ["Field", "Contents"], rows: [["exit_code", "Child exit status"], ["stdout", "Standard-output String"], ["stderr", "Standard-error String"], ["duration", "Command Duration"]] },
          { type: "p", text: "A nonzero exit_code does not automatically become an HHY Error; interpret it according to the command. stdout_lines(result) exposes captured output as a line Stream." }
        ] },
        { title: "Process snapshots and fields", blocks: [
          { type: "p", text: "processes() returns a current Stream<Process> snapshot. Process is not a Map and exposes read-only pid, name, cpu, memory, status, and command fields. Explicitly map ordinary fields before JSON encoding." },
          { type: "note", text: "sort_by({ order: \"desc\" }) accepts only asc or desc (default asc). It is a stable barrier that materializes the finite snapshot." }
        ] },
        { title: "args, env, system, and stdin", blocks: [
          { type: "table", columns: ["Value", "Contents"], rows: [["args", "List<String> excluding the script path"], ["env", "Read-only environment view"], ["system", "OS, architecture, host, CPU, memory, and directories"], ["stdin_lines()", "Standard-input line Stream"]] },
          { type: "code", language: "hhy", code: "if length(args) != 1 {\n    print_error(\"usage: script.hhy <input>\")\n    exit(3)\n}\n\nlet input = path(args[0])" }
        ] },
        { title: "Look up the complete API", blocks: [{ type: "link", href: "/en/learn/standard-library#fn-run", label: "Processes and system API Reference →", description: "Look up complete signatures for run, shell, processes, stdin_lines, and every." }] }
      ]
    }
  },
  {
    slug: "http",
    order: 7,
    title: { zh: "HTTP", en: "HTTP" },
    summary: { zh: "构建请求，配置超时与重试，并处理响应。", en: "Build requests, configure timeout and retry, and process responses." },
    sections: {
      zh: [
        { title: "Request → Policy → Send → Response", blocks: [
          { type: "code", language: "hhy", code: code.http },
          { type: "p", text: "http.get/post/put/delete(url, options?) 只创建不可变 HttpRequest，不访问网络。timeout(request, duration) 与 retry(request, options) 返回修改策略后的新 Request；send(request) 才产生 network effect 并返回 HttpResponse。这样 dry-run 能完整展示而不执行请求。" }
        ] },
        { title: "请求选项与安全默认值", blocks: [
          { type: "table", columns: ["选项", "用途"], rows: [["query", "URL 查询参数"], ["headers", "请求 header"], ["body", "请求内容"], ["proxy", "代理地址"], ["redirect", "重定向策略"]] },
          { type: "p", text: "TLS 验证默认开启。Authorization 和 Cookie 等敏感 header 会在计划、日志和 Error 中脱敏。响应体受 max_http_body 限制。" }
        ] },
        { title: "超时、重试与幂等性", blocks: [
          { type: "p", text: "retry({ count, backoff }) 默认只重试连接错误、timeout、429 和部分 5xx。GET、PUT、DELETE 可以按策略重试；POST 默认不自动重试，避免重复创建或扣款。timeout 和 Ctrl+C 都会取消 libcurl 操作并清理响应资源。" },
          { type: "note", text: "重试不是让失败消失。为每个请求设置 timeout，谨慎评估 POST 幂等性，并让最终错误保留 URL（脱敏）、方法、尝试次数和 Flow stage。" }
        ] },
        { title: "HttpResponse 与响应 body", blocks: [
          { type: "p", text: "send 返回 HttpResponse。使用 response_body 读取 UTF-8 文本，使用 response_bytes 读取图片、压缩包等二进制。两者都会验证响应状态；非成功状态产生 HttpStatusError。" },
          { type: "code", language: "hhy", code: "http.get(\"https://api.example.com/status\")\n    |> timeout(3s)\n    |> send\n    |> response_body\n    |> parse_json\n    |> print" }
        ] },
        { title: "查阅完整 API", blocks: [{ type: "link", href: "/zh/learn/standard-library#fn-http-get", label: "HTTP API Reference →", description: "查阅请求构造、timeout、retry、send 和响应读取函数。" }] }
      ],
      en: [
        { title: "Request → Policy → Send → Response", blocks: [
          { type: "code", language: "hhy", code: code.http },
          { type: "p", text: "http.get/post/put/delete(url, options?) only build immutable HttpRequests without network access. timeout(request, duration) and retry(request, options) return new policy-adjusted Requests; send(request) performs the network effect and returns HttpResponse. Dry-run can therefore inspect the full plan without execution." }
        ] },
        { title: "Request options and safe defaults", blocks: [
          { type: "table", columns: ["Option", "Purpose"], rows: [["query", "URL query parameters"], ["headers", "Request headers"], ["body", "Request content"], ["proxy", "Proxy address"], ["redirect", "Redirect policy"]] },
          { type: "p", text: "TLS verification is enabled by default. Sensitive Authorization and Cookie headers are redacted in plans, logs, and Errors. Response bodies obey max_http_body." }
        ] },
        { title: "Timeout, retries, and idempotency", blocks: [
          { type: "p", text: "retry({ count, backoff }) defaults to connection errors, timeouts, 429, and selected 5xx statuses. GET, PUT, and DELETE may retry by policy; POST does not retry automatically to avoid duplicate creation or charges. Timeout and Ctrl+C cancel libcurl work and release response resources." },
          { type: "note", text: "Retries do not erase failure. Give every request a timeout, evaluate POST idempotency, and preserve method, redacted URL, attempts, and Flow stage in the final Error." }
        ] },
        { title: "HttpResponse and response bodies", blocks: [
          { type: "p", text: "send returns HttpResponse. Use response_body for UTF-8 text and response_bytes for images, archives, and other binary data. Both validate response status; non-success raises HttpStatusError." },
          { type: "code", language: "hhy", code: "http.get(\"https://api.example.com/status\")\n    |> timeout(3s)\n    |> send\n    |> response_body\n    |> parse_json\n    |> print" }
        ] },
        { title: "Look up the complete API", blocks: [{ type: "link", href: "/en/learn/standard-library#fn-http-get", label: "HTTP API Reference →", description: "Look up request builders, timeout, retry, send, and response readers." }] }
      ]
    }
  },
  {
    slug: "parallel-watch",
    order: 8,
    title: { zh: "并发与监听", en: "Parallel and Watch" },
    summary: { zh: "有界并发处理、取消和文件事件流。", en: "Bounded parallel work, cancellation, and filesystem event streams." },
    sections: {
      zh: [
        { title: "parallel 是有界并发 map", blocks: [
          { type: "code", language: "hhy", code: code.parallel },
          { type: "table", columns: ["行为", "保证"], rows: [["并发上限", "最多运行 n 个 worker，并受 RuntimeLimits 约束"], ["输出顺序", "与输入顺序一致"], ["背压", "输入队列和结果缓冲都有界"], ["错误", "首个未处理 Error 取消剩余任务"], ["返回值", "等同并发 map，不自动展开子 Stream"]] },
          { type: "p", text: "如果闭包返回 Stream，在 parallel 后显式使用 flat_map。dry-run 不创建 worker，但仍会按顺序检查闭包中的 Effect。" }
        ] },
        { title: "Sendable 与隔离", blocks: [
          { type: "p", text: "worker 收到输入和闭包捕获值的冻结快照，不共享可变对象。Null、Bool、数字、String、单位、Path，以及字段均可发送的普通 List/Map/系统快照可复制过去。" },
          { type: "note", text: `捕获 let mut Cell、Stream、打开的 File handle、请求 body stream 或其他进程内资源会产生 CheckError。HHY ${hhyVersionLabel} 不公开线程、锁或 async/await。` }
        ] },
        { title: "watch 与 FileEvent", blocks: [
          { type: "code", language: "hhy", code: code.watch },
          { type: "p", text: "watch(path, { recursive? }) 返回无限 Stream<FileEvent>。FileEvent 有 kind、path、old_path、timestamp 只读字段；kind 是 created、modified、removed 或 renamed，old_path 仅 renamed 时存在。" },
          { type: "table", columns: ["字段", "内容"], rows: [["kind", "created、modified、removed 或 renamed"], ["path", "事件目标 Path"], ["old_path", "renamed 的原 Path，其余事件为 null"], ["timestamp", "事件 DateTime"]] },
          { type: "p", text: "watch 是无限 Stream。使用 Ctrl+C、timeout 或 cancel 结束监听；递归监听受 max_open_files 限制。底层文件系统可能合并短时间内的重复事件。" }
        ] },
        { title: "debounce 与 every", blocks: [
          { type: "p", text: "debounce(window) 使用 leading-edge：某个值或同一 kind + path 的 FileEvent 第一项立即输出，窗口内重复项被合并，并从最后一次重复重新计时；不同事件 key 互不阻塞。" },
          { type: "p", text: "every(duration) 返回无限 tick Stream。若下游还在处理，上游遵守背压，不重叠执行同一个 tick。定时和监听流进入 collect/sort/group 前必须先有 take 或业务窗口。" }
        ] },
        { title: "取消与清理", blocks: [
          { type: "p", text: "Ctrl+C、timeout、cancel() 和未处理错误触发同一个根 CancellationToken。watcher、sleep、HTTP、子进程与 worker 定期检查它；取消后关闭队列和句柄、终止子进程，并让 Stream close 从下游传播到上游。" }
        ] }
      ],
      en: [
        { title: "parallel is a bounded concurrent map", blocks: [
          { type: "code", language: "hhy", code: code.parallel },
          { type: "table", columns: ["Behavior", "Guarantee"], rows: [["Concurrency", "At most n workers, subject to RuntimeLimits"], ["Output order", "Matches input order"], ["Backpressure", "Input and result buffers are bounded"], ["Errors", "The first unhandled Error cancels remaining work"], ["Return", "Concurrent map; child Streams are not flattened"]] },
          { type: "p", text: "If a closure returns Stream, apply flat_map after parallel. Dry-run creates no workers but still checks closure Effects in order." }
        ] },
        { title: "Sendable values and isolation", blocks: [
          { type: "p", text: "Workers receive frozen snapshots of input and captured values, never shared mutable objects. Null, Bool, numbers, String, units, Path, and ordinary Lists/Maps/system snapshots whose fields are sendable can be copied." },
          { type: "note", text: `Capturing a let mut Cell, Stream, open File handle, request body stream, or other process-local resource raises CheckError. ${hhyVersionLabel} exposes no threads, locks, or async/await.` }
        ] },
        { title: "watch and FileEvent", blocks: [
          { type: "code", language: "hhy", code: code.watch },
          { type: "p", text: "watch(path, { recursive? }) returns an infinite Stream<FileEvent>. FileEvent exposes read-only kind, path, old_path, and timestamp; kind is created, modified, removed, or renamed, and old_path exists only for renamed." },
          { type: "table", columns: ["Field", "Contents"], rows: [["kind", "created, modified, removed, or renamed"], ["path", "Target Path"], ["old_path", "Original Path for renamed; otherwise null"], ["timestamp", "Event DateTime"]] },
          { type: "p", text: "watch is an infinite Stream. End it with Ctrl+C, timeout, or cancel. Recursive watch obeys max_open_files, and filesystems may merge rapid duplicate events." }
        ] },
        { title: "debounce and every", blocks: [
          { type: "p", text: "debounce(window) is leading-edge: the first scalar or kind + path FileEvent emits immediately; duplicates inside the window are coalesced and reset the window. Different event keys do not block each other." },
          { type: "p", text: "every(duration) returns an infinite tick Stream. If downstream is busy, backpressure prevents overlapping the same tick. Apply take or a business window before collect/sort/group." }
        ] },
        { title: "Cancellation and cleanup", blocks: [
          { type: "p", text: "Ctrl+C, timeout, cancel(), and unhandled errors trigger one root CancellationToken. Watchers, sleep, HTTP, child processes, and workers poll it; cancellation closes queues and handles, terminates children, and propagates Stream close upstream." }
        ] }
      ]
    }
  },
  {
    slug: "modules-errors",
    order: 9,
    title: { zh: "模块与错误", en: "Modules and Errors" },
    summary: { zh: "组织代码，传播结构化错误并可靠清理资源。", en: "Organize code, propagate structured errors, and unwind resources reliably." },
    sections: {
      zh: [
        { title: "导入形式与路径解析", blocks: [
          { type: "code", language: "hhy", code: code.module },
          { type: "table", columns: ["写法", "用途"], rows: [["import \"./lib/report.hhy\" as report", "导入本地模块命名空间"], ["import { parse } from \"./lib/data.hhy\"", "具名导入"], ["import { validate as check } from \"./lib/data.hhy\"", "具名导入并设置别名"], ["import http", "导入标准库模块"]] },
          { type: "p", text: "相对路径基于当前源码文件目录。标准库使用裸名称；本地文件显式使用 ./、../ 或绝对 Path。" }
        ] },
        { title: "export、作用域与执行", blocks: [
          { type: "code", language: "hhy", code: "export let version = \"1.0\"\n\nexport fn normalize_name(name) {\n    return name |> trim |> lower\n}\n\nfn internal_helper() {\n    return null\n}" },
          { type: "p", text: `只有 export 名称对外可见。模块拥有独立顶层作用域，并在首次 import 时执行一次、随后缓存。循环依赖在执行前产生 CheckError。${hhyVersionLabel} 支持标准库、本地模块，以及通过本地包安装的进程扩展模块。` }
        ] },
        { title: "Error 的字段与类别", blocks: [
          { type: "p", text: "所有失败都使用 Error，而不是靠 null 或打印文本表达。Error 提供 kind、code、message、source、stage、cause、stack、context 字段；敏感 header、凭据和完整文件内容不会默认进入 context。" },
          { type: "p", text: "内置类别包括 SyntaxError、CheckError、TypeError、ValueError、IndexError、KeyError、EncodingError、IoError、ProcessError、HttpError、HttpStatusError、TimeoutError、CancelledError、ResourceLimitError 和 PlanError。" }
        ] },
        { title: "try/catch 与重新抛出", blocks: [
          { type: "code", language: "hhy", code: code.errors },
          { type: "p", text: "catch 捕获 try 块传播出的第一个错误。catch 正常结束后脚本继续；无法处理时用 throw(err) 保留错误链重新抛出。未处理错误让脚本以非零状态退出。" }
        ] },
        { title: "Flow 错误与单项 Result", blocks: [
          { type: "code", language: "hhy", code: "path(\"./configs\")\n    |> files(\"**/*.json\")\n    |> map { file -> attempt { read_text(file.path) } }\n    |> where { result -> result.ok }\n    |> map { result -> result.value }\n    |> print" },
          { type: "p", text: "Stream 中未处理的 Error 默认终止整条 Pipeline。attempt 把单次操作转换为 Result，适合批处理中显式保留成功项和失败项。on_error 用于替换整条失败的上游 Stream，不会自动跳过错误项。" }
        ] },
        { title: "资源清理保证", blocks: [
          { type: "p", text: "Error、return、exit、timeout、Ctrl+C 和 cancel 都走统一 unwind。Stream close 幂等；原子保存失败会删除临时文件并保留旧文件；子进程、HTTP response、watcher 和 worker 都会响应取消。" }
        ] }
      ],
      en: [
        { title: "Import forms and path resolution", blocks: [
          { type: "code", language: "hhy", code: code.module },
          { type: "table", columns: ["Form", "Use"], rows: [["import \"./lib/report.hhy\" as report", "Import a local module namespace"], ["import { parse } from \"./lib/data.hhy\"", "Named import"], ["import { validate as check } from \"./lib/data.hhy\"", "Named import with alias"], ["import http", "Import a standard-library module"]] },
          { type: "p", text: "Relative paths resolve from the current source file. Standard modules use bare names; local files explicitly use ./, ../, or an absolute Path." }
        ] },
        { title: "Exports, scope, and execution", blocks: [
          { type: "code", language: "hhy", code: "export let version = \"1.0\"\n\nexport fn normalize_name(name) {\n    return name |> trim |> lower\n}\n\nfn internal_helper() {\n    return null\n}" },
          { type: "p", text: `Only exported names are visible to importers. A module owns its top-level scope, executes once on first import, and is then cached. Import cycles raise CheckError before execution. ${hhyVersionLabel} supports standard and local modules plus process-extension modules installed from local packages.` }
        ] },
        { title: "Error fields and categories", blocks: [
          { type: "p", text: "Every failure uses Error rather than null or printed text. Error exposes kind, code, message, source, stage, cause, stack, and context. Sensitive headers, credentials, and full file contents are excluded from context by default." },
          { type: "p", text: "Built-in categories include SyntaxError, CheckError, TypeError, ValueError, IndexError, KeyError, EncodingError, IoError, ProcessError, HttpError, HttpStatusError, TimeoutError, CancelledError, ResourceLimitError, and PlanError." }
        ] },
        { title: "try/catch and rethrowing", blocks: [
          { type: "code", language: "hhy", code: code.errors },
          { type: "p", text: "catch receives the first Error propagated from try. Execution continues after a normally completed catch; use throw(err) to preserve the error chain when it cannot be handled. An unhandled Error exits nonzero." }
        ] },
        { title: "Flow errors and per-item Result", blocks: [
          { type: "code", language: "hhy", code: "path(\"./configs\")\n    |> files(\"**/*.json\")\n    |> map { file -> attempt { read_text(file.path) } }\n    |> where { result -> result.ok }\n    |> map { result -> result.value }\n    |> print" },
          { type: "p", text: "An unhandled Error in a Stream terminates the Pipeline. attempt converts one operation to Result for batches that explicitly retain successes and failures. on_error replaces an entirely failed upstream Stream and never silently skips an item." }
        ] },
        { title: "Resource cleanup guarantees", blocks: [
          { type: "p", text: "Error, return, exit, timeout, Ctrl+C, and cancel share one unwind path. Stream close is idempotent; failed atomic saves delete temporary files and preserve the old file; child processes, HTTP responses, watchers, and workers respond to cancellation." }
        ] }
      ]
    }
  },
  {
    slug: "syntax-reference",
    order: 14,
    title: { zh: "语法完整参考", en: "Complete Syntax Reference" },
    summary: { zh: `${hhyVersionLabel} 的词法、字面量、运算符、语句、闭包和模块语法。`, en: `${hhyVersionLabel} lexical rules, literals, operators, statements, closures, and module syntax.` },
    sections: {
      zh: [
        { title: "源文件与词法", blocks: [
          { type: "table", columns: ["项目", "规则"], rows: [["文件", ".hhy、UTF-8、LF 或 CRLF"], ["标识符", "大小写敏感；ASCII 字母、数字和下划线，不能以数字开头"], ["语句结束", "换行或可选分号"], ["续行", "未闭合括号或行首/行尾的 |>"], ["注释", "# 单行注释；首行允许 shebang"], ["/", "表达式起点为 Regex，已有左操作数后为除法"]] }
        ] },
        { title: "字面量与原生单位", blocks: [
          { type: "code", language: "hhy", code: code.syntaxLiterals },
          { type: "p", text: "Range 包含起点、不包含终点。Bytes 支持 b/kb/mb/gb/kib/mib/gib；Duration 支持 ns/us/ms/s/min/h；紧贴数字的 % 是 Percent。String 支持插值以及 \\, \", \\n, \\r, \\t, \\b, \\f, \\0 转义。" }
        ] },
        { title: "运算符优先级（高到低）", blocks: [
          { type: "code", language: "text", code: code.syntaxOperators },
          { type: "p", text: "and、or 与 ?? 短路执行；= 只允许给 let mut 绑定赋值；|> 左结合。条件必须是 Bool，不存在把 0、空字符串或 null 自动当作 false 的规则。" }
        ] },
        { title: "声明、控制流、函数与模块", blocks: [
          { type: "code", language: "hhy", code: code.syntaxStatements },
          { type: "table", columns: ["结构", "形式"], rows: [["调用", "name(args)"], ["Pipe", "x |> f(a) 等价于 f(x, a)"], ["闭包", "{ param -> expression }"], ["Map", "{ key: value }"], ["控制流", "if、for、while、break、continue、return"], ["模块", "import、as、export"]] }
        ] },
        { title: "核心值类型", blocks: [
          { type: "code", language: "text", code: "Null Bool Int Float String Regex BytesBuffer\nList Map Range Function Error Result Stream\nBytes Duration Percent DateTime Path\nFile Directory FileEvent Process CommandResult\nHttpRequest HttpResponse" },
          { type: "note", text: "HHY 是动态类型语言，但不会进行危险的 String/Number 或 String/Bool 隐式转换。Int 是有符号 64 位整数，Float 是 IEEE 754 double。" }
        ] }
      ],
      en: [
        { title: "Source files and lexical rules", blocks: [
          { type: "table", columns: ["Item", "Rule"], rows: [["File", ".hhy, UTF-8, LF or CRLF"], ["Identifier", "Case-sensitive ASCII letters, digits, and underscores; cannot start with a digit"], ["Statement end", "Newline or optional semicolon"], ["Continuation", "Open delimiters or leading/trailing |>"], ["Comment", "# line comment; first line may be a shebang"], ["/", "Regex at expression start, division after a left operand"]] }
        ] },
        { title: "Literals and native units", blocks: [
          { type: "code", language: "hhy", code: code.syntaxLiterals },
          { type: "p", text: "Ranges include the start and exclude the end. Bytes support b/kb/mb/gb/kib/mib/gib; Duration supports ns/us/ms/s/min/h; % attached to a number creates Percent. Strings support interpolation and \\, \", \\n, \\r, \\t, \\b, \\f, and \\0 escapes." }
        ] },
        { title: "Operator precedence (highest to lowest)", blocks: [
          { type: "code", language: "text", code: code.syntaxOperators },
          { type: "p", text: "and, or, and ?? short-circuit; = may assign only to a let mut binding; |> is left-associative. Conditions require Bool—0, empty strings, and null are not implicitly false." }
        ] },
        { title: "Declarations, control flow, functions, and modules", blocks: [
          { type: "code", language: "hhy", code: code.syntaxStatements },
          { type: "table", columns: ["Construct", "Form"], rows: [["Call", "name(args)"], ["Pipe", "x |> f(a) equals f(x, a)"], ["Closure", "{ param -> expression }"], ["Map", "{ key: value }"], ["Control flow", "if, for, while, break, continue, return"], ["Module", "import, as, export"]] }
        ] },
        { title: "Core value types", blocks: [
          { type: "code", language: "text", code: "Null Bool Int Float String Regex BytesBuffer\nList Map Range Function Error Result Stream\nBytes Duration Percent DateTime Path\nFile Directory FileEvent Process CommandResult\nHttpRequest HttpResponse" },
          { type: "note", text: "HHY is dynamically typed but does not perform dangerous String/Number or String/Bool coercions. Int is signed 64-bit and Float is IEEE 754 double." }
        ] }
      ]
    }
  },
  {
    slug: "standard-library",
    order: 15,
    title: { zh: "标准库函数索引", en: "Standard Library Function Index" },
    summary: { zh: `运行时 Registry 中全部 94 个 ${hhyVersionLabel} 核心 callable 的签名与用途。`, en: `Signatures and purposes for all 94 ${hhyVersionLabel} core callables in the runtime Registry.` },
    sections: {
      zh: [
        { title: "如何阅读签名", blocks: [
          { type: "p", text: `本页以 ${hhyVersionLabel} Runtime 的 Callable Contract Registry 为权威来源，共 94 个核心 callable；扩展动态注册的 callable 在各扩展文档中说明。T/U 表示泛型占位值，? 表示可选参数或可空结果，Map? 表示可选 options Map。所有函数都可普通调用；在管道中，左侧值会注入为第一个参数。` },
          { type: "note", text: "这是完整 callable 清单，不含 args、env、system 等只读特殊值，也不把 File.path、HttpResponse.status 等只读字段误列为函数。" }
        ] },
        { title: "核心值、集合、环境与控制（22）", blocks: [callableList("stdCore", "zh")] },
        { title: "Flow 与 Stream（25）", blocks: [
          { type: "p", text: "map/where/take 等转换保持惰性；collect、count、reduce 等终端操作消费 Stream；sort_by 与 group_by 会在资源上限内物化输入。parallel 使用有界隔离 worker 并保持输出顺序。" },
          callableList("stdFlow", "zh")
        ] },
        { title: "文本、Regex、JSON 与 CSV（17）", blocks: [callableList("stdText", "zh")] },
        { title: "路径、文件与监听（15）", blocks: [
          { type: "p", text: "read_* 是读取操作；write_* 直接写入；save_* 使用临时文件加原子替换。文件系统 action 会被 dry-run 拦截。" },
          callableList("stdFiles", "zh")
        ] },
        { title: "进程、标准输入与定时（6）", blocks: [
          { type: "p", text: "run 直接传递 argv，不经过 Shell；只有 shell 明确采用 Shell 解析。进程启动会受 timeout、输出和进程数限制。" },
          callableList("stdProcess", "zh")
        ] },
        { title: "HTTP（9）", blocks: [
          { type: "p", text: "http.* 只构造不可变请求计划，timeout/retry 修改计划，只有 send 产生网络副作用。response_body 返回 UTF-8 文本，二进制响应使用 response_bytes。" },
          callableList("stdHttp", "zh")
        ] }
      ],
      en: [
        { title: "Reading the signatures", blocks: [
          { type: "p", text: `This page is sourced from the ${hhyVersionLabel} Runtime Callable Contract Registry and contains all 94 core callables; dynamically registered callables are documented by their extensions. T/U are generic placeholders, ? marks an optional argument or nullable result, and Map? is an optional options Map. Every function supports ordinary calls; a pipe injects its left value as the first argument.` },
          { type: "note", text: "This is the complete callable list. It excludes read-only special values such as args, env, and system, and does not mislabel read-only fields such as File.path or HttpResponse.status as functions." }
        ] },
        { title: "Core values, collections, environment, and control (22)", blocks: [callableList("stdCore", "en")] },
        { title: "Flow and Stream (25)", blocks: [
          { type: "p", text: "Transformations such as map, where, and take stay lazy; terminals such as collect, count, and reduce consume the Stream. sort_by and group_by materialize input within resource limits. parallel uses bounded isolated workers and preserves output order." },
          callableList("stdFlow", "en")
        ] },
        { title: "Text, Regex, JSON, and CSV (17)", blocks: [callableList("stdText", "en")] },
        { title: "Paths, files, and watch (15)", blocks: [
          { type: "p", text: "read_* functions read data, write_* functions write directly, and save_* functions use a temporary file plus atomic replacement. Dry-run intercepts filesystem actions." },
          callableList("stdFiles", "en")
        ] },
        { title: "Processes, standard input, and timers (6)", blocks: [
          { type: "p", text: "run passes argv directly without a shell; only shell explicitly uses shell parsing. Process launches obey timeout, output, and process-count limits." },
          callableList("stdProcess", "en")
        ] },
        { title: "HTTP (9)", blocks: [
          { type: "p", text: "http.* only builds immutable request plans, timeout/retry transform a plan, and only send performs a network effect. response_body returns UTF-8 text; use response_bytes for binary data." },
          callableList("stdHttp", "en")
        ] }
      ]
    }
  },
  {
    slug: "practical-recipes",
    order: 10,
    title: { zh: "实战：可直接落地的自动化", en: "Practical Automation Recipes" },
    summary: { zh: "完整收录 examples/00–08，并增加发布门禁、安全审计、订单对账、租户快照和素材治理。", en: "Every example from examples/00–08, plus release gates, security audits, reconciliation, tenant snapshots, and asset governance." },
    sections: {
      zh: [
        { title: "00 · Hello HHY 与 Flow 入门", blocks: [
          { type: "p", text: "最小可运行案例：把 List 转成 Stream，依次完成映射和过滤，最后输出结果。对应 examples/00-hello.hhy。" },
          { type: "code", language: "hhy", code: code.hello, filename: "00-hello.hhy" },
          { type: "terminal", command: "hhy run examples/00-hello.hhy", output: "HHY: Flow\nHHY: Pipe\nHHY: System\n\n✓ exit 0 · Flow 管道执行完成" }
        ] },
        { title: "并发提取日志告警", blocks: [
          { type: "p", text: "递归扫描大日志文件，使用 4 个 worker 提取 ERROR/WARN 行，并将来源文件写入结果。适合服务器日志归档、故障排查和定时任务。" },
          { type: "code", language: "hhy", code: code.practicalLogs, filename: "log-errors.hhy" },
          { type: "code", language: "sh", code: "hhy run log-errors.hhy ./logs ./output/errors.txt" },
          { type: "terminal", command: "hhy run log-errors.hhy ./logs ./output/errors.txt && head -3 ./output/errors.txt", output: "logs/api.log: 2026-08-25T09:18:42Z ERROR database timeout after 3000ms\nlogs/worker.log: 2026-08-25T09:18:44Z WARN retrying job #1842\nlogs/api.log: 2026-08-25T09:18:47Z ERROR upstream returned 502\n\n✓ exit 0 · 3 条告警已写入 output/errors.txt" }
        ] },
        { title: "从 API 同步活跃用户", blocks: [
          { type: "p", text: "请求用户接口，经过超时、重试、JSON 解析和字段裁剪后，只把活跃用户原子写入本地文件。对应 examples/02-active-users.hhy。" },
          { type: "code", language: "hhy", code: code.practicalActiveUsers, filename: "active-users.hhy" },
          { type: "code", language: "sh", code: "hhy run active-users.hhy https://api.example.com/users active-users.json" },
          { type: "terminal", command: "hhy run active-users.hhy http://127.0.0.1:9000/users active-users.json && cat active-users.json", output: "[\n  { \"id\": 101, \"name\": \"Ada\", \"email\": \"ada@example.com\" },\n  { \"id\": 108, \"name\": \"Linus\", \"email\": \"linus@example.com\" }\n]\n\n✓ exit 0 · 2 位活跃用户已写入 active-users.json" }
        ] },
        { title: "进程 CPU / 内存监控", blocks: [
          { type: "p", text: "每 5 秒读取一次进程快照，找出 CPU 超过 70% 或内存超过 1 GiB 的进程，并按内存倒序输出前 10 个。对应 examples/03-process-monitor.hhy。" },
          { type: "code", language: "hhy", code: code.practicalProcesses, filename: "03-process-monitor.hhy" },
          { type: "terminal", command: "hhy run examples/03-process-monitor.hhy", output: "[{ pid: 8421, name: \"node\", cpu: 82.4%, memory: 1.42 GiB },\n { pid: 9107, name: \"hhy\",  cpu: 74.1%, memory: 86.3 MiB }]\n\nnext sample in 5s… · Ctrl+C 安全退出" }
        ] },
        { title: "批量服务健康检查", blocks: [
          { type: "p", text: "并发探测多个服务，统一设置超时与重试；单个接口失败时记录错误，不中断整批巡检。可接入发布检查或 CI。" },
          { type: "code", language: "hhy", code: code.practicalHealth, filename: "health-check.hhy" },
          { type: "terminal", command: "hhy run health-check.hhy", output: "[\n  { \"name\": \"users\",   \"ok\": true,  \"status\": \"healthy\", \"error\": null },\n  { \"name\": \"orders\",  \"ok\": true,  \"status\": \"healthy\", \"error\": null },\n  { \"name\": \"billing\", \"ok\": false, \"status\": \"unreachable\", \"error\": \"request timed out\" }\n]\n\n✓ exit 0 · 3 个服务并发完成，单点失败未中断批次" }
        ] },
        { title: "业务进阶 01 · 发布质量门禁", blocks: [
          { type: "p", text: "在发布前并行执行测试、Lint 和生产构建，生成机器可读报告；任一检查失败就用稳定退出码阻止发布。适合 CI/CD、灰度发布和交付验收。" },
          { type: "code", language: "hhy", code: code.businessReleaseGate, filename: "release-gate.hhy" },
          { type: "terminal", command: "hhy run release-gate.hhy", output: "unit-tests       PASS  4.28s\nlint             PASS  1.14s\nproduction-build PASS  6.72s\nrelease-gate.json written\n\n✓ exit 0 · release gate passed" }
        ] },
        { title: "业务进阶 02 · 源码敏感信息审计", blocks: [
          { type: "p", text: "并发扫描配置与源码，定位疑似 API Key、密码和私钥内容，汇总成可供安全团队复核的报告。适合提交前检查和合规巡检。" },
          { type: "code", language: "hhy", code: code.businessSecretAudit, filename: "secret-audit.hhy" },
          { type: "terminal", command: "hhy run secret-audit.hhy ./services secret-findings.txt", output: "services/billing/.env: PAYMENT_API_KEY=***\nservices/auth/config.yml: PASSWORD: ***\n\n✓ exit 0 · 2 条疑似敏感信息待复核" },
          { type: "note", text: "示例输出已脱敏。实际落地时应限制报告权限，并在流水线中避免打印秘密原文。" }
        ] },
        { title: "业务进阶 03 · 订单与支付自动对账", blocks: [
          { type: "p", text: "把订单 CSV 与支付 CSV 合并为一条数据流，按 order_id 分组，找出缺少记录或金额不一致的异常订单。适合每日财务对账。" },
          { type: "code", language: "hhy", code: code.businessReconcile, filename: "reconcile.hhy" },
          { type: "terminal", command: "hhy run reconcile.hhy orders.csv payments.csv exceptions.json", output: "orders: 12,480 · payments: 12,472\nmatched: 12,461\nexceptions: 19 → exceptions.json\n\n✓ exit 0 · 对账报告已原子写入" }
        ] },
        { title: "业务进阶 04 · SaaS 多租户用量快照", blocks: [
          { type: "p", text: "有界并发拉取各租户用量，统一处理超时和重试；单租户失败被隔离并记录，不影响整份快照生成。适合计费、容量分析和客户成功报表。" },
          { type: "code", language: "hhy", code: code.businessTenantSnapshot, filename: "tenant-snapshot.hhy" },
          { type: "terminal", command: "hhy run tenant-snapshot.hhy", output: "acme  ✓ requests=184203 storage_gb=82.4\nnova  ✓ requests=99102  storage_gb=41.8\norbit ✗ request timed out\n\n✓ exit 0 · tenant-usage-snapshot.json 包含成功数据与失败原因" }
        ] },
        { title: "业务进阶 05 · 大体积素材治理", blocks: [
          { type: "p", text: "遍历图片与视频素材，筛选超过 5 MiB 的文件并按体积倒序生成 JSON 清单，帮助内容团队定位需要压缩或迁移的资产。" },
          { type: "code", language: "hhy", code: code.businessAssetAudit, filename: "asset-audit.hhy" },
          { type: "terminal", command: "hhy run asset-audit.hhy ./public asset-report.json", output: "scanned 1,842 assets\nlarge assets: 27\nlargest: public/video/launch.mp4 · 184.2 MiB\n\n✓ exit 0 · asset-report.json 已生成" }
        ] },
        { title: "监听源码并自动构建", blocks: [
          { type: "p", text: "监听 C 代码变化，通过 debounce 合并短时间内的连续保存，再执行 make。构建失败只打印错误，监听任务继续运行。" },
          { type: "code", language: "hhy", code: code.practicalWatch, filename: "watch-build.hhy" },
          { type: "code", language: "sh", code: "hhy run watch-build.hhy ./src" },
          { type: "terminal", command: "hhy run watch-build.hhy ./src", output: "watching ./src recursively…\nchanged: src/runtime/flow.c\ncc -std=c11 -O2 -c src/runtime/flow.c\ncc build/*.o -lcurl -lpcre2-8 -lgc -o build/hhy\nBuild complete: build/hhy\n\n✓ watcher remains active · waiting for the next change" }
        ] },
        { title: "从 CSV 生成部门汇总报表", blocks: [
          { type: "p", text: "读取员工 CSV，筛选在职人员，按部门统计人数和薪资总额，最后原子写入格式化 JSON。输入列为 name、department、active、salary。" },
          { type: "code", language: "hhy", code: code.practicalCsv, filename: "csv-report.hhy" },
          { type: "code", language: "sh", code: "hhy run csv-report.hhy employees.csv department-report.json" },
          { type: "terminal", command: "hhy run csv-report.hhy employees.csv department-report.json && cat department-report.json", output: "[\n  { \"department\": \"Engineering\", \"employees\": 12, \"total_salary\": 2160000 },\n  { \"department\": \"Product\", \"employees\": 5, \"total_salary\": 810000 }\n]\n\n✓ exit 0 · department-report.json 已原子写入" }
        ] },
        { title: "大文件备份（支持 dry-run）", blocks: [
          { type: "p", text: "找出超过 100 MiB 的文件并复制到备份目录。先用 dry-run 检查动作计划，确认无误后再真实执行。" },
          { type: "code", language: "hhy", code: code.practicalBackup, filename: "backup-large.hhy" },
          { type: "code", language: "sh", code: "hhy run --dry-run backup-large.hhy ./downloads ./backup\nhhy run backup-large.hhy ./downloads ./backup" },
          { type: "terminal", command: "hhy run --dry-run backup-large.hhy ./downloads ./backup", output: "copy downloads/archive.tar -> backup/archive.tar\ncopy downloads/database.dump -> backup/database.dump\n[dry-run] copy downloads/archive.tar → backup/archive.tar\n[dry-run] copy downloads/database.dump → backup/database.dump\n\n✓ exit 0 · 仅生成计划，没有写入文件" },
          { type: "note", text: "备份脚本默认不覆盖同名文件，并自动创建目标目录；正式执行前仍建议先运行 dry-run。" }
        ] },
        { title: "07 · 语言基础综合练习", blocks: [
          { type: "p", text: "用一个小型汇总任务串起变量、List、Map、函数、条件、循环、作用域和错误处理。对应 examples/07-language-basics.hhy。" },
          { type: "code", language: "hhy", code: code.basics, filename: "07-language-basics.hhy" },
          { type: "terminal", command: "hhy run examples/07-language-basics.hhy", output: "{\n  \"count\": 2,\n  \"total\": 40,\n  \"average\": 20\n}\n\n✓ exit 0 · 汇总结果已生成" }
        ] }
      ],
      en: [
        { title: "00 · Hello HHY and Flow", blocks: [
          { type: "p", text: "The smallest runnable example: turn a List into a Stream, map and filter values, then print the result. Corresponds to examples/00-hello.hhy." },
          { type: "code", language: "hhy", code: code.hello, filename: "00-hello.hhy" },
          { type: "terminal", command: "hhy run examples/00-hello.hhy", output: "HHY: Flow\nHHY: Pipe\nHHY: System\n\n✓ exit 0 · Flow pipeline completed" }
        ] },
        { title: "Extract log alerts concurrently", blocks: [
          { type: "p", text: "Recursively scan large log files with four workers, extract ERROR/WARN lines, and retain each source path. Useful for incident response and scheduled log jobs." },
          { type: "code", language: "hhy", code: code.practicalLogs, filename: "log-errors.hhy" },
          { type: "code", language: "sh", code: "hhy run log-errors.hhy ./logs ./output/errors.txt" },
          { type: "terminal", command: "hhy run log-errors.hhy ./logs ./output/errors.txt && head -3 ./output/errors.txt", output: "logs/api.log: 2026-08-25T09:18:42Z ERROR database timeout after 3000ms\nlogs/worker.log: 2026-08-25T09:18:44Z WARN retrying job #1842\nlogs/api.log: 2026-08-25T09:18:47Z ERROR upstream returned 502\n\n✓ exit 0 · 3 alerts written to output/errors.txt" }
        ] },
        { title: "Sync active users from an API", blocks: [
          { type: "p", text: "Fetch users with timeout and retry, parse JSON, select fields, and atomically save only active users. Corresponds to examples/02-active-users.hhy." },
          { type: "code", language: "hhy", code: code.practicalActiveUsers, filename: "active-users.hhy" },
          { type: "code", language: "sh", code: "hhy run active-users.hhy https://api.example.com/users active-users.json" },
          { type: "terminal", command: "hhy run active-users.hhy http://127.0.0.1:9000/users active-users.json && cat active-users.json", output: "[\n  { \"id\": 101, \"name\": \"Ada\", \"email\": \"ada@example.com\" },\n  { \"id\": 108, \"name\": \"Linus\", \"email\": \"linus@example.com\" }\n]\n\n✓ exit 0 · 2 active users written to active-users.json" }
        ] },
        { title: "Monitor process CPU and memory", blocks: [
          { type: "p", text: "Sample processes every five seconds, keep CPU-heavy or memory-heavy entries, and print the top ten by memory. Corresponds to examples/03-process-monitor.hhy." },
          { type: "code", language: "hhy", code: code.practicalProcesses, filename: "03-process-monitor.hhy" },
          { type: "terminal", command: "hhy run examples/03-process-monitor.hhy", output: "[{ pid: 8421, name: \"node\", cpu: 82.4%, memory: 1.42 GiB },\n { pid: 9107, name: \"hhy\",  cpu: 74.1%, memory: 86.3 MiB }]\n\nnext sample in 5s… · Ctrl+C exits safely" }
        ] },
        { title: "Check service health in batches", blocks: [
          { type: "p", text: "Probe multiple services concurrently with consistent timeouts and retries. A failed endpoint is recorded without terminating the whole batch." },
          { type: "code", language: "hhy", code: code.practicalHealth, filename: "health-check.hhy" },
          { type: "terminal", command: "hhy run health-check.hhy", output: "[\n  { \"name\": \"users\",   \"ok\": true,  \"status\": \"healthy\", \"error\": null },\n  { \"name\": \"orders\",  \"ok\": true,  \"status\": \"healthy\", \"error\": null },\n  { \"name\": \"billing\", \"ok\": false, \"status\": \"unreachable\", \"error\": \"request timed out\" }\n]\n\n✓ exit 0 · all 3 checks completed despite one endpoint failure" }
        ] },
        { title: "Business 01 · Release quality gate", blocks: [
          { type: "p", text: "Run tests, lint, and production builds in parallel, save a machine-readable report, and block a release with a stable exit code when any check fails." },
          { type: "code", language: "hhy", code: code.businessReleaseGate, filename: "release-gate.hhy" },
          { type: "terminal", command: "hhy run release-gate.hhy", output: "unit-tests       PASS  4.28s\nlint             PASS  1.14s\nproduction-build PASS  6.72s\nrelease-gate.json written\n\n✓ exit 0 · release gate passed" }
        ] },
        { title: "Business 02 · Source secret audit", blocks: [
          { type: "p", text: "Scan configuration and source files concurrently for suspected API keys, passwords, and private keys, then produce a security review report." },
          { type: "code", language: "hhy", code: code.businessSecretAudit, filename: "secret-audit.hhy" },
          { type: "terminal", command: "hhy run secret-audit.hhy ./services secret-findings.txt", output: "services/billing/.env: PAYMENT_API_KEY=***\nservices/auth/config.yml: PASSWORD: ***\n\n✓ exit 0 · 2 suspected secrets require review" },
          { type: "note", text: "The example output is redacted. Restrict report access and avoid printing raw secrets in production pipelines." }
        ] },
        { title: "Business 03 · Order and payment reconciliation", blocks: [
          { type: "p", text: "Merge order and payment CSV files into one flow, group records by order_id, and report missing records or mismatched amounts." },
          { type: "code", language: "hhy", code: code.businessReconcile, filename: "reconcile.hhy" },
          { type: "terminal", command: "hhy run reconcile.hhy orders.csv payments.csv exceptions.json", output: "orders: 12,480 · payments: 12,472\nmatched: 12,461\nexceptions: 19 → exceptions.json\n\n✓ exit 0 · reconciliation report written atomically" }
        ] },
        { title: "Business 04 · Multi-tenant usage snapshot", blocks: [
          { type: "p", text: "Fetch tenant usage with bounded concurrency, consistent retries, and failure isolation. Useful for billing, capacity analysis, and customer success reports." },
          { type: "code", language: "hhy", code: code.businessTenantSnapshot, filename: "tenant-snapshot.hhy" },
          { type: "terminal", command: "hhy run tenant-snapshot.hhy", output: "acme  ✓ requests=184203 storage_gb=82.4\nnova  ✓ requests=99102  storage_gb=41.8\norbit ✗ request timed out\n\n✓ exit 0 · snapshot contains both data and failure reasons" }
        ] },
        { title: "Business 05 · Oversized asset governance", blocks: [
          { type: "p", text: "Find large image and video assets, sort them by size, and produce a JSON inventory for compression or storage migration work." },
          { type: "code", language: "hhy", code: code.businessAssetAudit, filename: "asset-audit.hhy" },
          { type: "terminal", command: "hhy run asset-audit.hhy ./public asset-report.json", output: "scanned 1,842 assets\nlarge assets: 27\nlargest: public/video/launch.mp4 · 184.2 MiB\n\n✓ exit 0 · asset-report.json generated" }
        ] },
        { title: "Watch sources and rebuild", blocks: [
          { type: "p", text: "Watch C sources, debounce rapid saves, and run make. Build failures are reported while the watcher stays alive." },
          { type: "code", language: "hhy", code: code.practicalWatch, filename: "watch-build.hhy" },
          { type: "code", language: "sh", code: "hhy run watch-build.hhy ./src" },
          { type: "terminal", command: "hhy run watch-build.hhy ./src", output: "watching ./src recursively…\nchanged: src/runtime/flow.c\ncc -std=c11 -O2 -c src/runtime/flow.c\ncc build/*.o -lcurl -lpcre2-8 -lgc -o build/hhy\nBuild complete: build/hhy\n\n✓ watcher remains active · waiting for the next change" }
        ] },
        { title: "Build a department report from CSV", blocks: [
          { type: "p", text: "Read employee CSV records, keep active employees, aggregate headcount and salary by department, and atomically save formatted JSON." },
          { type: "code", language: "hhy", code: code.practicalCsv, filename: "csv-report.hhy" },
          { type: "code", language: "sh", code: "hhy run csv-report.hhy employees.csv department-report.json" },
          { type: "terminal", command: "hhy run csv-report.hhy employees.csv department-report.json && cat department-report.json", output: "[\n  { \"department\": \"Engineering\", \"employees\": 12, \"total_salary\": 2160000 },\n  { \"department\": \"Product\", \"employees\": 5, \"total_salary\": 810000 }\n]\n\n✓ exit 0 · department-report.json written atomically" }
        ] },
        { title: "Back up large files with dry-run", blocks: [
          { type: "p", text: "Find files over 100 MiB and copy them into a backup directory. Inspect the plan with dry-run before performing real writes." },
          { type: "code", language: "hhy", code: code.practicalBackup, filename: "backup-large.hhy" },
          { type: "code", language: "sh", code: "hhy run --dry-run backup-large.hhy ./downloads ./backup\nhhy run backup-large.hhy ./downloads ./backup" },
          { type: "terminal", command: "hhy run --dry-run backup-large.hhy ./downloads ./backup", output: "copy downloads/archive.tar -> backup/archive.tar\ncopy downloads/database.dump -> backup/database.dump\n[dry-run] copy downloads/archive.tar → backup/archive.tar\n[dry-run] copy downloads/database.dump → backup/database.dump\n\n✓ exit 0 · plan generated without writing files" },
          { type: "note", text: "The recipe refuses to overwrite files and creates parent directories, but you should still inspect the dry-run plan first." }
        ] },
        { title: "07 · Language basics in one task", blocks: [
          { type: "p", text: "A small aggregation task combining variables, Lists, Maps, functions, conditions, loops, scopes, and error handling. Corresponds to examples/07-language-basics.hhy." },
          { type: "code", language: "hhy", code: code.basics, filename: "07-language-basics.hhy" },
          { type: "terminal", command: "hhy run examples/07-language-basics.hhy", output: "{\n  \"count\": 2,\n  \"total\": 40,\n  \"average\": 20\n}\n\n✓ exit 0 · summary generated" }
        ] }
      ]
    }
  },
  {
    slug: "cli-reference",
    order: 16,
    title: { zh: "CLI 参考", en: "CLI Reference" },
    summary: { zh: "运行、检查、格式化、REPL 与 dry-run。", en: "Run, check, format, use the REPL, and inspect dry-run plans." },
    sections: {
      zh: [
        { title: "版本与发布信息", blocks: [
          { type: "p", text: "使用 --version 确认当前二进制版本、项目作者、开源许可证和官方联系方式。源码构建使用 ./build/hhy；发行包或安装到 PATH 后可直接使用 hhy。" },
          { type: "terminal-card", title: "HHY · 版本信息", command: "./build/hhy --version", output: `hhy ${hhyVersion}\n© 2026 HHY Language contributors\nAuthor: houhuiyang\nLicense: Apache License 2.0\nhttps://hhylang.dev/\nhuiyang.hou@qq.com`, caption: `HHY ${hhyVersion} 的真实命令输出；版本、作者、许可证、官网和联系邮箱由 CLI 直接提供。` },
          { type: "note", text: "如果从正式发行包运行，请在解压目录执行 ./bin/hhy --version；如果已经 make install 或加入 PATH，则执行 hhy --version。" }
        ] },
        { title: "完整命令", blocks: [
          { type: "code", language: "sh", code: code.cli },
          { type: "table", columns: ["命令", "用途"], rows: [["hhy run", "运行脚本并传递 args"], ["hhy repl", "启动交互环境"], ["hhy check", "检查语法和核心语义"], ["hhy fmt", "写入官方格式"], ["hhy fmt --check", "只检查格式"], ["hhy ast", "输出 AST"], ["hhy tokens", "输出 Lexer Token"], ["hhy run --dry-run", "预览脱敏执行计划"]] },
          { type: "p", text: "hhy script.hhy 是 hhy run script.hhy 的简写。脚本参数可能以 - 开头时，在 Runtime 选项后使用 -- 分隔。" }
        ] },
        { title: "Runtime 资源限制", blocks: [
          { type: "p", text: "run 的 --limit NAME=VALUE 可以重复出现。大小必须带 b/kb/mb/gb/kib/mib/gib，时间必须带 ns/us/ms/s/min/h，计数值不带单位。" },
          { type: "code", language: "sh", code: "hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy" },
          { type: "table", columns: ["限制", "默认值"], rows: [["max_memory", "512mib"], ["max_open_files", "256"], ["max_processes", "16"], ["max_parallelism", "16"], ["max_http_body", "16mib"], ["max_regex_steps", "1000000"], ["max_recursion", "256"], ["max_runtime", "0（CLI 默认不设总时限）"]] }
        ] },
        { title: "稳定退出码", blocks: [
          { type: "code", language: "text", code: "0  成功\n1  未处理的运行时错误\n2  语法或静态检查错误\n3  CLI 用法错误\n4  文件 I/O、进程或网络错误\n5  超时或取消" },
          { type: "p", text: "自动化脚本应按稳定退出码而不是错误文本进行分支。" }
        ] }
      ],
      en: [
        { title: "Version and release identity", blocks: [
          { type: "p", text: "Use --version to confirm the binary version, project author, open-source license, and official contact details. Source builds use ./build/hhy; release archives or PATH installations can invoke hhy directly." },
          { type: "terminal-card", title: "HHY · Version information", command: "./build/hhy --version", output: `hhy ${hhyVersion}\n© 2026 HHY Language contributors\nAuthor: houhuiyang\nLicense: Apache License 2.0\nhttps://hhylang.dev/\nhuiyang.hou@qq.com`, caption: `Actual HHY ${hhyVersion} command output. The CLI reports the version, author, license, website, and contact address directly.` },
          { type: "note", text: "From an official archive, run ./bin/hhy --version in the extracted directory. After make install or adding HHY to PATH, run hhy --version." }
        ] },
        { title: "Complete command set", blocks: [
          { type: "code", language: "sh", code: code.cli },
          { type: "table", columns: ["Command", "Purpose"], rows: [["hhy run", "Run a script and pass args"], ["hhy repl", "Start the interactive environment"], ["hhy check", "Check syntax and core semantics"], ["hhy fmt", "Write canonical formatting"], ["hhy fmt --check", "Check formatting only"], ["hhy ast", "Print the AST"], ["hhy tokens", "Print Lexer tokens"], ["hhy run --dry-run", "Preview a redacted execution plan"]] },
          { type: "p", text: "hhy script.hhy is shorthand for hhy run script.hhy. Use -- after Runtime options when script arguments may begin with a dash." }
        ] },
        { title: "Runtime resource limits", blocks: [
          { type: "p", text: "The run command accepts repeatable --limit NAME=VALUE options. Sizes require b/kb/mb/gb/kib/mib/gib, durations require ns/us/ms/s/min/h, and counts have no unit." },
          { type: "code", language: "sh", code: "hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy" },
          { type: "table", columns: ["Limit", "Default"], rows: [["max_memory", "512mib"], ["max_open_files", "256"], ["max_processes", "16"], ["max_parallelism", "16"], ["max_http_body", "16mib"], ["max_regex_steps", "1000000"], ["max_recursion", "256"], ["max_runtime", "0 (no total CLI limit)"]] }
        ] },
        { title: "Stable exit codes", blocks: [
          { type: "code", language: "text", code: "0  success\n1  unhandled runtime error\n2  syntax or static-check error\n3  invalid CLI usage\n4  file I/O, process, or network error\n5  timeout or cancellation" },
          { type: "p", text: "Automation should branch on stable exit codes rather than error text." }
        ] }
      ]
    }
  },
  {
    slug: "dataflow-etl-project",
    order: 12,
    title: { zh: "实战项目：DataFlow ETL", en: "Project: DataFlow ETL" },
    summary: { zh: "从 CSV、JSON 目录和 HTTP API 同步数据，完成清洗、过滤、并发补全、分组汇总及 JSON/CSV 双输出。", en: "Synchronize CSV, JSON-directory, and HTTP API data through cleaning, filtering, concurrent enrichment, grouping, and JSON/CSV outputs." },
    sections: {
      zh: [
        { title: "完整的数据同步管道", blocks: [
          { type: "p", text: `DataFlow ETL 是完全由 HHY ${hhyVersionTag} 运行并通过端到端自测的数据同步应用。它读取客户 CSV 和事件 JSON 目录，规范化姓名与邮箱，过滤停用和低消费客户，并发请求本地画像 API，按部门 group_by 汇总，最后原子写入 JSON 报告和 CSV 明细。` },
          { type: "table", columns: ["阶段", "实现"], rows: [["采集", "read_lines + parse_csv；files + parse_json"], ["清洗", "trim、lower、to_int 与结构化 Map"], ["补全", "parallel(4) + http.get + timeout + retry"], ["汇总", "where、sort_by、group_by、sum"], ["输出", "encode_json/save_text 与 encode_csv/save_lines"]] },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/tree/main/dataflow-etl", label: "在 GitHub 查看 DataFlow ETL 完整源码 ↗", description: "包含 HHY 模块、CSV/JSON 测试数据、画像 API、报告断言和一键自测。" }
        ] },
        { title: "真实目录与数据流", blocks: [
          { type: "image", src: "/dataflow-etl-tree.png", alt: "DataFlow ETL 项目目录树", caption: "真实项目目录：入口、四个 HHY 模块、CSV/JSON fixtures、配置和测试工具。", width: 900, height: 760, size: "medium" },
          { type: "code", language: "text", code: "customers.csv + events/*.json + HTTP profiles\n                    ↓\n            parse / trim / lower\n                    ↓\n          active + minimum spend filter\n                    ↓\n          parallel HTTP enrichment\n                    ↓\n          group_by department + sum\n                    ↓\n              report.json + customers.csv" }
        ] },
        { title: "实际自测结果", blocks: [
          { type: "code", language: "sh", code: "cd hhy-vm\nsh dataflow-etl/self-test.sh" },
          { type: "image", src: "/dataflow-etl-self-test.png", alt: "DataFlow ETL 实际端到端自测结果", caption: "真实运行：3 条合格客户、2 个事件文件、2 个部门汇总，HTTP 补全及 JSON/CSV 断言全部通过。", width: 1180, height: 400, size: "wide" },
          { type: "note", text: "测试服务只监听 127.0.0.1:18992。测试会验证客户过滤和排序、邮箱清洗、远程 region/tier 字段、部门消费汇总以及两种输出格式。" }
        ] },
        { title: "运行自己的同步任务", blocks: [
          { type: "p", text: "复制 config/test.json，替换项目名、API 地址和最低消费阈值，再准备 customers.csv 与 events/*.json。HTTP 单项失败会变成结构化 error，报告 ok=false 并返回退出码 1。" },
          { type: "code", language: "sh", code: "hhy run dataflow-etl/etl.hhy \\\n  ./input \\\n  ./config.json \\\n  ./output/report.json \\\n  ./output/customers.csv" }
        ] }
      ],
      en: [
        { title: "A complete synchronization pipeline", blocks: [
          { type: "p", text: `DataFlow ETL runs entirely on HHY ${hhyVersionTag} and is verified end to end. It reads customer CSV and an event JSON directory, normalizes names and email addresses, filters inactive and low-spend customers, calls a profile API concurrently, aggregates departments with group_by, and atomically writes JSON and CSV outputs.` },
          { type: "table", columns: ["Stage", "Implementation"], rows: [["Ingest", "read_lines + parse_csv; files + parse_json"], ["Clean", "trim, lower, to_int, and structured Maps"], ["Enrich", "parallel(4) + http.get + timeout + retry"], ["Aggregate", "where, sort_by, group_by, and sum"], ["Output", "encode_json/save_text and encode_csv/save_lines"]] },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/tree/main/dataflow-etl", label: "View the complete DataFlow ETL source on GitHub ↗", description: "Includes HHY modules, CSV/JSON fixtures, profile API, report assertions, and one-command self-test." }
        ] },
        { title: "Real layout and data flow", blocks: [
          { type: "image", src: "/dataflow-etl-tree.png", alt: "DataFlow ETL project tree", caption: "The real project layout: entry point, four HHY modules, CSV/JSON fixtures, configuration, and test utilities.", width: 900, height: 760, size: "medium" },
          { type: "code", language: "text", code: "customers.csv + events/*.json + HTTP profiles\n                    ↓\n            parse / trim / lower\n                    ↓\n          active + minimum spend filter\n                    ↓\n          parallel HTTP enrichment\n                    ↓\n          group_by department + sum\n                    ↓\n              report.json + customers.csv" }
        ] },
        { title: "Actual self-test result", blocks: [
          { type: "code", language: "sh", code: "cd hhy-vm\nsh dataflow-etl/self-test.sh" },
          { type: "image", src: "/dataflow-etl-self-test.png", alt: "Actual DataFlow ETL end-to-end result", caption: "Actual run: three qualified customers, two event files, two department summaries, and all HTTP enrichment plus JSON/CSV assertions passed.", width: 1180, height: 400, size: "wide" },
          { type: "note", text: "The test API binds only to 127.0.0.1:18992. Assertions cover filtering and ordering, email cleanup, remote region/tier fields, department spend totals, and both output formats." }
        ] },
        { title: "Run your own synchronization", blocks: [
          { type: "p", text: "Copy config/test.json, replace the project name, API base, and minimum-spend threshold, then provide customers.csv and events/*.json. An individual HTTP failure becomes a structured error, sets report ok=false, and returns exit code 1." },
          { type: "code", language: "sh", code: "hhy run dataflow-etl/etl.hhy \\\n  ./input \\\n  ./config.json \\\n  ./output/report.json \\\n  ./output/customers.csv" }
        ] }
      ]
    }
  },
  {
    slug: "asset-governance-project",
    order: 13,
    title: { zh: "实战项目：Asset Governance", en: "Project: Asset Governance" },
    summary: { zh: "扫描项目资产、生成治理报告，并用 Runtime 原生 dry-run 安全执行 copy、move、remove 整改动作。", en: "Audit project assets, generate a governance report, and safely execute copy, move, and remove remediations with Runtime-native dry-run." },
    sections: {
      zh: [
        { title: "审计与整改分离", blocks: [
          { type: "p", text: "Asset Governance 由 audit.hhy 和 cleanup.hhy 两个程序组成。审计器扫描源码、配置、图片、视频和构建产物，发现超大、过旧、命名不规范、重复文本内容和疑似凭据；清理器只接受审计报告中的白名单动作，不通过 shell 拼接命令。" },
          { type: "table", columns: ["检查或动作", "HHY 实现"], rows: [["文件清单和大小", "files、File.size、Bytes"], ["旧文件", "File.modified、now、Duration"], ["命名与敏感信息", "Regex、read_text、脱敏 finding"], ["重复内容", "group_by 文本内容，不把原文写进报告"], ["整改", "copy、move、remove 与 --dry-run EffectDispatcher"]] },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/tree/main/asset-governance", label: "在 GitHub 查看 Asset Governance 完整源码 ↗", description: "包含审计器、清理器、四个 HHY 模块、风险 fixtures 和 dry-run/正式整改断言。" }
        ] },
        { title: "项目目录", blocks: [
          { type: "image", src: "/asset-governance-tree.png", alt: "Asset Governance 项目目录树", caption: "真实目录包含审计与清理入口、治理模块，以及故意准备的大文件、旧文件、重复文件和敏感配置 fixtures。", width: 900, height: 960, size: "medium" },
          { type: "table", columns: ["程序", "职责"], rows: [["audit.hhy", "扫描项目并原子生成 report.json；存在 critical finding 时返回 1"], ["cleanup.hhy", "读取 report.actions，执行受控 copy/move/remove"], ["self-test.sh", "创建隔离 mktemp 工作区，先 dry-run 再正式整改并逐项断言"]] }
        ] },
        { title: "实际自测与 dry-run", blocks: [
          { type: "code", language: "sh", code: "cd hhy-vm\nsh asset-governance/self-test.sh" },
          { type: "image", src: "/asset-governance-self-test.png", alt: "Asset Governance 审计、dry-run 和正式整改的真实终端输出", caption: "真实运行：识别 large/naming/stale/sensitive/duplicate finding；dry-run 输出副作用计划且文件不变；正式执行三个动作后断言通过。", width: 1180, height: 1100, size: "wide" },
          { type: "note", text: "截图中的 Processed 表示程序走到了该动作；dry-run 阶段由 Runtime 拦截副作用。测试随后确认目标目录完全未改变，正式运行后才验证 copy、move 和 remove 生效。" }
        ] },
        { title: "两阶段运行", blocks: [
          { type: "p", text: "先运行审计并阅读 report.json。存在 critical finding 时 audit 返回 1，但报告仍完整生成。确认 actions 后先执行 dry-run，检查 Runtime 输出的 effect 计划，最后再正式整改。" },
          { type: "code", language: "sh", code: "hhy run asset-governance/audit.hhy ./project ./config.json ./report.json\nhhy run --dry-run asset-governance/cleanup.hhy ./project ./report.json\nhhy run asset-governance/cleanup.hhy ./project ./report.json" }
        ] }
      ],
      en: [
        { title: "Separate audit from remediation", blocks: [
          { type: "p", text: "Asset Governance consists of audit.hhy and cleanup.hhy. The auditor scans source, configuration, images, video, and build outputs for oversized, stale, badly named, duplicate-text, and possibly sensitive files. The cleaner accepts only allow-listed report actions and never assembles shell commands." },
          { type: "table", columns: ["Check or action", "HHY implementation"], rows: [["Inventory and size", "files, File.size, and Bytes"], ["Stale files", "File.modified, now, and Duration"], ["Naming and secrets", "Regex, read_text, and redacted findings"], ["Duplicate content", "group_by text content without storing source text in the report"], ["Remediation", "copy, move, remove, and --dry-run EffectDispatcher"]] },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/tree/main/asset-governance", label: "View the complete Asset Governance source on GitHub ↗", description: "Includes auditor, cleaner, four HHY modules, risky fixtures, and dry-run plus applied-remediation assertions." }
        ] },
        { title: "Project layout", blocks: [
          { type: "image", src: "/asset-governance-tree.png", alt: "Asset Governance project tree", caption: "The real layout contains audit and cleanup entry points, governance modules, and intentionally large, stale, duplicate, and sensitive fixtures.", width: 900, height: 960, size: "medium" },
          { type: "table", columns: ["Program", "Responsibility"], rows: [["audit.hhy", "Scan and atomically write report.json; return 1 when a critical finding exists"], ["cleanup.hhy", "Read report.actions and execute controlled copy/move/remove operations"], ["self-test.sh", "Create an isolated mktemp workspace, dry-run first, then apply and assert every action"]] }
        ] },
        { title: "Actual self-test and dry-run", blocks: [
          { type: "code", language: "sh", code: "cd hhy-vm\nsh asset-governance/self-test.sh" },
          { type: "image", src: "/asset-governance-self-test.png", alt: "Actual Asset Governance audit, dry-run, and applied-remediation terminal output", caption: "Actual run: detects large/naming/stale/sensitive/duplicate findings; dry-run prints the effect plan without changes; all assertions pass after three real actions.", width: 1180, height: 1100, size: "wide" },
          { type: "note", text: "Processed means that program control reached the action; Runtime intercepts its effect during dry-run. The test then proves that the workspace is unchanged, and verifies copy, move, and remove only after the real run." }
        ] },
        { title: "Two-phase operation", blocks: [
          { type: "p", text: "Run the audit and inspect report.json first. audit returns 1 for a critical finding but still writes the complete report. After approving actions, run dry-run, inspect Runtime's effect plan, and only then apply remediation." },
          { type: "code", language: "sh", code: "hhy run asset-governance/audit.hhy ./project ./config.json ./report.json\nhhy run --dry-run asset-governance/cleanup.hhy ./project ./report.json\nhhy run asset-governance/cleanup.hhy ./project ./report.json" }
        ] }
      ]
    }
  },
  {
    slug: "flowguard-project",
    order: 11,
    title: { zh: "实战项目：FlowGuard", en: "Project: FlowGuard" },
    summary: { zh: `用 HHY ${hhyVersionTag} 构建完整的代码仓库体检与质量门禁应用，包含真实数据、并发检查、JSON 报告和端到端测试。`, en: `Build a complete repository health and quality-gate application with HHY ${hhyVersionTag}, including real fixtures, concurrent checks, JSON reports, and end-to-end tests.` },
    sections: {
      zh: [
        { title: "这不是语法 Demo", blocks: [
          { type: "p", text: `FlowGuard 是一个使用 HHY ${hhyVersionTag} 运行并通过自测的完整应用。它接收项目目录与 JSON 配置，检查必需文件、扫描文件和疑似凭据、并发执行质量命令与 HTTP 健康检查，最终原子写入结构化报告，并用稳定退出码决定质量门禁是否通过。` },
          { type: "table", columns: ["应用能力", "使用的 HHY 能力"], rows: [["项目结构检查", "Path、read_text、attempt、List"], ["文件与安全扫描", "files、Stream、Regex、Bytes"], ["质量命令", "run、parallel、Duration、CommandResult"], ["服务健康检查", "http.get、timeout、retry、parallel"], ["报告与门禁", "Map、encode_json、原子 save_text、exit"]] },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/tree/main/flowguard", label: "在 GitHub 查看 FlowGuard 完整源码 ↗", description: "包含 HHY 入口、六个业务模块、配置、测试项目、HTTP 服务和报告断言。" }
        ] },
        { title: "项目目录", blocks: [
          { type: "p", text: "入口脚本只负责编排，具体检查被拆分到 lib 中；config 保存两套场景，fixtures 提供可以重复测试的项目数据。output 和 __pycache__ 被忽略，不进入仓库。" },
          { type: "image", src: "/flowguard-project-tree-v2.png", alt: "FlowGuard 项目目录树，展示 config、fixtures、lib、入口脚本和测试工具", caption: "FlowGuard 的真实目录结构；output 与 __pycache__ 是本地自测产物，不在 Git 中。", width: 900, height: 890, size: "medium" },
          { type: "table", columns: ["路径", "职责"], rows: [["flowguard.hhy", "读取参数和配置，组合检查，写报告并设置退出码"], ["lib/*.hhy", "结构、文件、安全、命令、健康和报告模块"], ["config/*.json", "健康与风险场景配置"], ["fixtures/*", "确定性的被检查项目数据"], ["self-test.sh", "启动测试服务并验证两个端到端场景"]] }
        ] },
        { title: "运行完整自测", blocks: [
          { type: "p", text: "在仓库根目录执行一条命令。自测会启动仅监听 127.0.0.1:18991 的临时 HTTP 服务，先检查 HHY 模块，再运行健康和风险两套场景，并用 Python 对生成的 JSON 报告做结构与结果断言。" },
          { type: "code", language: "sh", code: "cd hhy-vm\nsh flowguard/self-test.sh" },
          { type: "image", src: "/flowguard-self-test-v2.png", alt: "FlowGuard 端到端自测的终端输出，健康场景全部通过，风险场景发现五项失败，最终自测通过", caption: "真实运行结果：healthy-service 8 项通过；risky-service 正确发现 5 项失败；最终 FlowGuard self-test passed。", width: 1180, height: 850, size: "wide" }
        ] },
        { title: "健康场景与风险场景", blocks: [
          { type: "table", columns: ["场景", "输入数据", "预期结果"], rows: [["healthy-service", "README、LICENSE、package.json、源码、两个成功命令和 2xx 健康端点", "8 passed，退出码 0"], ["risky-service", "缺少 LICENSE、模拟 DEMO_TOKEN、失败命令和 404 端点", "5 failed，退出码 1；自测将这个非零状态视为正确结果"]] },
          { type: "note", text: "风险场景里的凭据是明确标注的假数据。FlowGuard 报告只保存文件名和 content_redacted: true，不保存匹配内容。" }
        ] },
        { title: "配置自己的项目", blocks: [
          { type: "code", language: "text", code: "{\n  \"project\": { \"name\": \"my-service\" },\n  \"required_files\": [\"README.md\", \"LICENSE\"],\n  \"limits\": { \"large_file\": \"4kib\" },\n  \"commands\": [\n    { \"name\": \"tests\", \"argv\": [\"npm\", \"test\"] }\n  ],\n  \"health_checks\": [\n    { \"name\": \"api\", \"url\": \"http://127.0.0.1:8080/health\" }\n  ]\n}" },
          { type: "p", text: "命令使用 argv 数组直接交给 run，不经过 shell 拼接；每个命令限制为 15 秒和 1 MiB 输出。当前示例接受 256b、1kib、4kib 或 1mib 文件阈值。" },
          { type: "code", language: "sh", code: "hhy run \\\n  --limit max_runtime=2min \\\n  --limit max_memory=256mib \\\n  --limit max_processes=8 \\\n  flowguard/flowguard.hhy \\\n  /path/to/project \\\n  flowguard/config/my-project.json \\\n  report.json" }
        ] },
        { title: "为什么它能代表 HHY", blocks: [
          { type: "p", text: "FlowGuard 把文件系统、进程、HTTP 和数据处理统一进同一条可靠工作流。单项失败通过 attempt 转换为结构化检查结果，不会阻止其他检查完成；parallel 提供有上限的并发；最终报告可以直接交给 CI/CD 读取。这正是 HHY 相比复杂 Shell 脚本最有辨识度的应用方向。" },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/blob/main/flowguard/README.zh-CN.md", label: "阅读 FlowGuard 中文使用说明 ↗", description: "查看配置字段、手动运行方式、测试设计与真实项目接入命令。" }
        ] }
      ],
      en: [
        { title: "More than a syntax demo", blocks: [
          { type: "p", text: `FlowGuard is a complete application run and self-tested with HHY ${hhyVersionTag}. It accepts a project directory and JSON configuration, checks required files, scans files and possible credentials, runs quality commands and HTTP health checks concurrently, atomically writes a structured report, and uses a stable exit code to enforce the quality gate.` },
          { type: "table", columns: ["Application capability", "HHY capabilities used"], rows: [["Project structure", "Path, read_text, attempt, and List"], ["File and security scan", "files, Stream, Regex, and Bytes"], ["Quality commands", "run, parallel, Duration, and CommandResult"], ["Service health", "http.get, timeout, retry, and parallel"], ["Report and gate", "Map, encode_json, atomic save_text, and exit"]] },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/tree/main/flowguard", label: "View the complete FlowGuard source on GitHub ↗", description: "Includes the HHY entry point, six business modules, configurations, fixtures, HTTP server, and report assertions." }
        ] },
        { title: "Project layout", blocks: [
          { type: "p", text: "The entry script focuses on orchestration while lib contains each check. Config holds two scenarios, and fixtures provides repeatable project data. output and __pycache__ are ignored and never committed." },
          { type: "image", src: "/flowguard-project-tree-v2.png", alt: "FlowGuard project tree showing config, fixtures, lib, entry script, and test utilities", caption: "The real FlowGuard layout. output and __pycache__ are local test artifacts and are not tracked by Git.", width: 900, height: 890, size: "medium" },
          { type: "table", columns: ["Path", "Responsibility"], rows: [["flowguard.hhy", "Read arguments and configuration, combine checks, write the report, and set the exit code"], ["lib/*.hhy", "Structure, file, security, command, health, and reporting modules"], ["config/*.json", "Healthy and risky scenario configurations"], ["fixtures/*", "Deterministic projects under inspection"], ["self-test.sh", "Start the test service and verify both end-to-end scenarios"]] }
        ] },
        { title: "Run the complete self-test", blocks: [
          { type: "p", text: "Run one command from the repository root. The test starts a temporary HTTP service bound only to 127.0.0.1:18991, checks the HHY modules, runs both scenarios, and uses Python assertions to validate the generated JSON reports." },
          { type: "code", language: "sh", code: "cd hhy-vm\nsh flowguard/self-test.sh" },
          { type: "image", src: "/flowguard-self-test-v2.png", alt: "FlowGuard end-to-end terminal output with the healthy scenario passing and five expected failures in the risky scenario", caption: "Actual output: healthy-service passes all eight checks; risky-service finds five failures; the run ends with FlowGuard self-test passed.", width: 1180, height: 850, size: "wide" }
        ] },
        { title: "Healthy and risky scenarios", blocks: [
          { type: "table", columns: ["Scenario", "Input data", "Expected result"], rows: [["healthy-service", "README, LICENSE, package.json, source, two successful commands, and a 2xx health endpoint", "8 passed and exit code 0"], ["risky-service", "Missing LICENSE, fake DEMO_TOKEN, failed command, and a 404 endpoint", "5 failed and exit code 1; the harness treats this nonzero status as correct"]] },
          { type: "note", text: "The credential in the risky fixture is explicitly fake. FlowGuard stores only the file name and content_redacted: true; matching content never enters the report." }
        ] },
        { title: "Configure your own project", blocks: [
          { type: "code", language: "text", code: "{\n  \"project\": { \"name\": \"my-service\" },\n  \"required_files\": [\"README.md\", \"LICENSE\"],\n  \"limits\": { \"large_file\": \"4kib\" },\n  \"commands\": [\n    { \"name\": \"tests\", \"argv\": [\"npm\", \"test\"] }\n  ],\n  \"health_checks\": [\n    { \"name\": \"api\", \"url\": \"http://127.0.0.1:8080/health\" }\n  ]\n}" },
          { type: "p", text: "Commands are passed directly to run as argv arrays and are never assembled through shell. Each command is limited to 15 seconds and 1 MiB of output. The current example accepts 256b, 1kib, 4kib, or 1mib file thresholds." },
          { type: "code", language: "sh", code: "hhy run \\\n  --limit max_runtime=2min \\\n  --limit max_memory=256mib \\\n  --limit max_processes=8 \\\n  flowguard/flowguard.hhy \\\n  /path/to/project \\\n  flowguard/config/my-project.json \\\n  report.json" }
        ] },
        { title: "Why it represents HHY", blocks: [
          { type: "p", text: "FlowGuard brings filesystem, process, HTTP, and data processing into one reliable workflow. attempt turns an individual failure into a structured check without preventing other checks from completing; parallel provides bounded concurrency; and CI/CD can consume the final report directly. This is where HHY is most distinct from a large shell script." },
          { type: "link", href: "https://github.com/hh696-wq/hhy-vm/blob/main/flowguard/README.md", label: "Read the FlowGuard guide ↗", description: "See configuration fields, manual commands, test design, and instructions for checking a real project." }
        ] }
      ]
    }
  },
  {
    slug: "extensions-roadmap",
    order: 17,
    title: { zh: "扩展系统与数据库扩展", en: "Extension System and Database Extension" },
    summary: { zh: "面向扩展开发者的 v1.1.0 实现说明：本地包、进程协议、加载链路和官方 C 数据库扩展。", en: "The v1.1.0 implementation guide for extension developers: local packages, the process protocol, load lifecycle, and the official C database extension." },
    sections: {
      zh: [
        { title: "v1.1.0 已实现的边界", blocks: [
          { type: "note", text: `${hhyVersionTag} 已实现本地 install/list/remove、manifest 与 SHA-256 校验、隔离进程握手、动态 callable 注册、同步调用、结构化错误和 shutdown。脚本可以直接 import 已安装的扩展包，例如 import database。` },
          { type: "table", columns: ["能力", "v1.1.0 状态", "边界"], rows: [["本地扩展包", "已实现", "仅本地路径；不从远程仓库下载"], ["进程协议", "已实现", "handshake、register、call、call_result、error、shutdown"], ["值传输", "已实现", "Null、Bool、数字、String、List、Map 的 JSON 协议映射"], ["Stream / handle / cancel", "未实现", "属于后续协议扩展"], ["公开 Native ABI", "未承诺", "只有进程协议无法满足且有性能证据时再评估"]] },
          { type: "p", text: "包名就是顶级命名空间。database 包只能注册 database.*；hhy.*、std.*、核心 callable 和其他包名不能被覆盖。导入未安装的包会得到 ModuleNotFoundError。" }
        ] },
        { title: "安装、查看与移除", blocks: [
          { type: "code", language: "sh", code: "make -C extensions/database\n./build/hhy install ./extensions/database\n./build/hhy list\n./build/hhy remove database" },
          { type: "table", columns: ["步骤", "实际行为"], rows: [["install", "读取 hhy.toml；校验包名、作者、requires_hhy、协议、命令和完整性；展示 capability 后由用户确认安装"], ["import / load", "重新校验已安装文件的 SHA-256，启动扩展进程，握手并注册 callable"], ["list", "显示已安装包的名称、版本、作者、协议和声明的 capability"], ["remove", "删除本地包记录和安装目录；之后 import 会失败"]] },
          { type: "note", text: "capability 是安装时可审查的权限声明。v1.1.0 尚未承诺通用的操作系统级进程沙箱；第三方扩展仍应按原生可执行文件对待，只安装可信来源。" }
        ] },
        { title: "真实的 database/hhy.toml", blocks: [
          { type: "p", text: "下面就是仓库中官方 database 0.2.0 扩展使用的清单，不是虚构草案。command 相对包根目录解析；network 同时声明 PostgreSQL 与 MySQL 的本机默认端口。" },
          { type: "code", language: "text", code: "[package]\nname = \"database\"\nversion = \"0.2.0\"\nauthor = \"HHY Official\"\nrequires_hhy = \">=1.1,<2.0\"\n\n[extension]\nkind = \"process\"\ncommand = \"bin/hhy-database\"\nprotocol = \"1\"\n\n[capabilities]\nread = []\nwrite = []\nnetwork = [\"127.0.0.1:5432\", \"127.0.0.1:3306\"]\nprocess = false" },
          { type: "table", columns: ["字段", "开发者约束"], rows: [["package.name", "唯一顶级命名空间；此处为 database"], ["package.author", "安装与 list 时展示，明确官方或第三方来源"], ["requires_hhy", "安装器检查 Runtime 版本范围"], ["extension.command", "必须是包内可执行文件，不能逃出包目录"], ["extension.protocol", "v1.1.0 只接受协议 1"], ["capabilities", "声明需要审查的文件、网络和子进程访问范围"]] }
        ] },
        { title: "扩展如何加载", blocks: [
          { type: "extension-flow" },
          { type: "table", columns: ["阶段", "Runtime 与扩展的职责"], rows: [["resolve", "Runtime 根据 import database 定位已安装包，解析清单并校验命令与完整性"], ["spawn", "Runtime 以 --protocol 1 启动独立进程，并建立 stdin/stdout 协议管道"], ["handshake", "双方确认 extension_id=database 与 protocol_version=1.0"], ["register", "database 发送一次注册消息；Runtime 验证命名空间和 contract 后写入 callable registry"], ["call", "Runtime 把可序列化参数发送给扩展；request_id 关联 call 与 call_result"], ["shutdown", "Runtime 发送 shutdown 并回收协议流和子进程"]] },
          { type: "note", text: "v1.1.0 是同步、逐次调用协议，不提供 Stream、Opaque handle 或协议级 cancel。文档不应把后续设计写成当前已支持能力。" }
        ] },
        { title: "官方 database 扩展示例", blocks: [
          { type: "p", text: "database 是仓库中真实存在的 C11 进程扩展，支持 PostgreSQL 与 MySQL。它使用数据库原生参数 API，不把参数拼接进 SQL；查询值以 String 或 Null 返回，避免数据库精度在协议转换中丢失。" },
          { type: "code", language: "hhy", filename: "database-example.hhy", code: "import database\n\nlet url = require_env(\"DATABASE_URL\")\n\ndatabase.ping(url) |> print\n\ndatabase.query(\n    url,\n    \"SELECT id, name FROM users WHERE active = ?\",\n    [true],\n    100\n) |> get(\"rows\") |> print\n\ndatabase.execute(\n    url,\n    \"UPDATE users SET active = ? WHERE id = ?\",\n    [false, 42]\n) |> print" },
          { type: "table", columns: ["已注册 callable", "参数与返回"], rows: [["database.ping(url)", "验证连接；返回 Map"], ["database.query(url, sql, params, max_rows?)", "执行有界参数化查询；返回 columns、rows 等结果 Map"], ["database.execute(url, sql, params)", "执行参数化写入；返回受影响行数等 Map"], ["database.transaction(url, statements)", "在一个连接中原子执行 1–100 条 INSERT/UPDATE/DELETE；失败时整体回滚"]] },
          { type: "note", text: "MySQL 占位符使用 ?；PostgreSQL 使用 $1、$2……。第一版 transaction 明确排除 DDL 和返回结果集的查询。连接 URL 应从 require_env 或受保护的本地配置读取，不要写入源码或提交仓库。" }
        ] },
        { title: "扩展作者需要实现什么", blocks: [
          { type: "table", columns: ["部分", "v1.1.0 要求"], rows: [["包", "提供 hhy.toml、包内可执行命令和安装器可验证的 SHA-256 完整性信息"], ["启动", "只接受 --protocol 1；协议消息只写 stdout，日志写 stderr"], ["handshake", "验证 extension_id 与 protocol_version，并返回匹配身份"], ["register", "恰好发送一次初始注册；名称必须位于包命名空间且 contract 完整"], ["call", "按 request_id 返回 call_result 或结构化 error，不泄露凭据、SQL 参数或敏感诊断"], ["shutdown", "幂等释放连接、内存和其他扩展资源"], ["测试", "至少覆盖协议身份不匹配、非法参数、扩展退出、数据库失败和资源清理"]] },
          { type: "p", text: "扩展进程不会继承完整宿主环境；Runtime 只通过协议传递脚本显式提供的参数。开发者应让错误可定位但不包含密码、连接 URL、SQL 参数或数据库敏感诊断。" }
        ] }
      ],
      en: [
        { title: "The implemented v1.1.0 boundary", blocks: [
          { type: "note", text: `${hhyVersionTag} implements local install/list/remove, manifest and SHA-256 validation, isolated-process handshakes, dynamic callable registration, synchronous calls, structured errors, and shutdown. Scripts can directly import an installed package, for example import database.` },
          { type: "table", columns: ["Capability", "v1.1.0 status", "Boundary"], rows: [["Local extension packages", "Implemented", "Local paths only; no remote registry download"], ["Process protocol", "Implemented", "handshake, register, call, call_result, error, shutdown"], ["Value transport", "Implemented", "JSON protocol mapping for Null, Bool, numbers, String, List, and Map"], ["Stream / handle / cancel", "Not implemented", "Reserved for a future protocol extension"], ["Public Native ABI", "Not committed", "Evaluate only if measurements show the process model is insufficient"]] },
          { type: "p", text: "The package name is its top-level namespace. The database package may register only database.*; hhy.*, std.*, core callables, and other package names cannot be replaced. Importing a package that is not installed raises ModuleNotFoundError." }
        ] },
        { title: "Install, list, and remove", blocks: [
          { type: "code", language: "sh", code: "make -C extensions/database\n./build/hhy install ./extensions/database\n./build/hhy list\n./build/hhy remove database" },
          { type: "table", columns: ["Step", "Actual behavior"], rows: [["install", "Read hhy.toml; validate package name, author, requires_hhy, protocol, command, and integrity; display capabilities and ask the user to confirm"], ["import / load", "Recheck installed SHA-256 data, start the extension process, handshake, and register callables"], ["list", "Display each installed package's name, version, author, protocol, and declared capabilities"], ["remove", "Delete the local package record and installation directory; subsequent imports fail"]] },
          { type: "note", text: "Capabilities are reviewable declarations shown during installation. v1.1.0 does not promise a general operating-system process sandbox; treat a third-party extension as a native executable and install only trusted packages." }
        ] },
        { title: "The real database/hhy.toml", blocks: [
          { type: "p", text: "This is the manifest used by the official database 0.2.0 package in the repository, not a hypothetical draft. command resolves from the package root; network declares the local default ports for PostgreSQL and MySQL." },
          { type: "code", language: "text", code: "[package]\nname = \"database\"\nversion = \"0.2.0\"\nauthor = \"HHY Official\"\nrequires_hhy = \">=1.1,<2.0\"\n\n[extension]\nkind = \"process\"\ncommand = \"bin/hhy-database\"\nprotocol = \"1\"\n\n[capabilities]\nread = []\nwrite = []\nnetwork = [\"127.0.0.1:5432\", \"127.0.0.1:3306\"]\nprocess = false" },
          { type: "table", columns: ["Field", "Developer constraint"], rows: [["package.name", "Unique top-level namespace; database here"], ["package.author", "Shown during install and list to identify official or third-party provenance"], ["requires_hhy", "Runtime version range checked by the installer"], ["extension.command", "Must be an executable inside the package and cannot escape its root"], ["extension.protocol", "v1.1.0 accepts protocol 1"], ["capabilities", "Declares file, network, and subprocess access for review"]] }
        ] },
        { title: "How an extension loads", blocks: [
          { type: "extension-flow" },
          { type: "table", columns: ["Stage", "Runtime and extension responsibility"], rows: [["resolve", "Runtime resolves import database to an installed package, parses its manifest, and validates command integrity"], ["spawn", "Runtime starts a separate process with --protocol 1 and opens stdin/stdout protocol pipes"], ["handshake", "Both sides confirm extension_id=database and protocol_version=1.0"], ["register", "database sends one registration message; Runtime validates the namespace and contracts before updating its callable registry"], ["call", "Runtime sends serializable arguments; request_id correlates each call and call_result"], ["shutdown", "Runtime sends shutdown and reaps protocol streams and the child process"]] },
          { type: "note", text: "v1.1.0 is a synchronous, one-call-at-a-time protocol. Stream transport, opaque handles, and protocol-level cancellation are not supported and must not be documented as current behavior." }
        ] },
        { title: "Official database extension example", blocks: [
          { type: "p", text: "database is a real C11 process extension in the repository with PostgreSQL and MySQL support. It uses each driver's native parameter API and never concatenates parameters into SQL. Query values return as String or Null so protocol conversion does not lose database precision." },
          { type: "code", language: "hhy", filename: "database-example.hhy", code: "import database\n\nlet url = require_env(\"DATABASE_URL\")\n\ndatabase.ping(url) |> print\n\ndatabase.query(\n    url,\n    \"SELECT id, name FROM users WHERE active = ?\",\n    [true],\n    100\n) |> get(\"rows\") |> print\n\ndatabase.execute(\n    url,\n    \"UPDATE users SET active = ? WHERE id = ?\",\n    [false, 42]\n) |> print" },
          { type: "table", columns: ["Registered callable", "Arguments and result"], rows: [["database.ping(url)", "Validate connectivity; returns a Map"], ["database.query(url, sql, params, max_rows?)", "Run a bounded parameterized query; returns a result Map containing columns and rows"], ["database.execute(url, sql, params)", "Run a parameterized write; returns a Map including affected-row information"], ["database.transaction(url, statements)", "Atomically execute 1–100 INSERT/UPDATE/DELETE statements on one connection; roll back all statements on failure"]] },
          { type: "note", text: "MySQL placeholders are ?; PostgreSQL placeholders are $1, $2, and so on. The first transaction API deliberately excludes DDL and result-returning queries. Read connection URLs from require_env or protected local configuration—never hard-code or commit credentials." }
        ] },
        { title: "What an extension author must implement", blocks: [
          { type: "table", columns: ["Part", "v1.1.0 requirement"], rows: [["Package", "Provide hhy.toml, an in-package executable command, and SHA-256 integrity data verifiable by the installer"], ["Startup", "Accept only --protocol 1; write protocol messages only to stdout and logs to stderr"], ["Handshake", "Validate extension_id and protocol_version and return matching identity"], ["Register", "Send exactly one initial registration; every name must stay in the package namespace and provide a valid contract"], ["Call", "Return call_result or structured error for each request_id without exposing credentials, SQL parameters, or sensitive diagnostics"], ["Shutdown", "Idempotently release connections, memory, and other extension resources"], ["Tests", "Cover identity mismatch, invalid arguments, extension exit, database failure, and resource cleanup"]] },
          { type: "p", text: "The extension process does not receive a copy of the complete host environment; Runtime passes only arguments explicitly supplied by the script over the protocol. Errors should remain actionable without including passwords, connection URLs, SQL parameters, or sensitive database diagnostics." }
        ] }
      ]
    }
  },
  {
    slug: "language-vm-roadmap",
    order: 18,
    title: { zh: "语言与 VM 演进路线图", en: "Language and VM Evolution Roadmap" },
    summary: { zh: "从 v1.2 到 v2.0 的五版本演进顺序、建议时间窗口、交付边界与进入条件。", en: "Five releases from v1.2 to v2.0, with recommended windows, delivery boundaries, and entry gates." },
    sections: {
      zh: [
        { title: "五版本演进总览", blocks: [
          { type: "note", text: "以下时间是基于依赖顺序、验证成本和兼容性风险给出的最佳实践窗口，不是发布日期承诺。每个版本只有在上一阶段验收条件通过后才进入冻结。" },
          { type: "evolution-roadmap" }
        ] },
        { title: "版本谱系、时间与验收门槛", blocks: [
          { type: "table", columns: ["版本", "建议窗口（非承诺）", "核心交付", "进入下一阶段前必须满足"], rows: [
            ["v1.0.0 · 已发布", "2026-08-25", "核心语言与 VM 语义冻结", "Pipe、Value、Stream、Error、核心标准库和三平台发行证据完成"],
            ["v1.1.0 · 已发布", "2026-08-26", "本地进程扩展与官方数据库扩展", "安装/加载完整性、Protocol 1 同步调用、database 0.2.0 和三平台发行证据完成"],
            ["v1.2", "2026 Q4–2027 Q1", "扩展协议补全与官方 Office 验证", "Stream credit、cancel、Opaque handle 生命周期和 capability 路径通过大工作簿压力测试"],
            ["v1.3", "2027 Q2", "数据库资源模型", "连接 handle/池、流式查询、类型映射和事务在 PostgreSQL/MySQL 上具有稳定资源上限与回归测试"],
            ["v1.4", "2027 Q3", "包分发与工程工具链", "签名校验、依赖解析、远程索引、离线锁定和可复现安装具备安全审计与回滚方案"],
            ["v1.5", "2027 Q4–2028 Q1", "Runtime 可观测性与长期稳定化", "trace/profile/debug hooks、性能基线、模糊测试、故障注入和兼容矩阵持续通过"],
            ["v2.0", "最早 2028 H2", "生态开放与 ABI 决策", "至少两个真实集成证明进程协议不足；否则继续使用进程协议并不开放 Native ABI"]
          ] }
        ] },
        { title: "演进原则", blocks: [
          { type: "table", columns: ["原则", "约束"], rows: [["语义先冻结", "Pipe、Value、Stream、Error 与取消语义先稳定，再扩展生态表面"], ["可用、可测先于高性能", "每项能力先具备确定错误、资源上限和跨平台测试，再进行优化"], ["协议优先", "第三方能力优先通过 Process Extension Protocol 接入，不并行发明第二套语言语义"], ["ABI 有条件开放", "Native ABI 只有在 Runtime 足够稳定且测量证明必要时才评估；不开放也是有效结论"]] },
          { type: "p", text: "路线图每个季度应重新评审一次：只调整尚未冻结版本；已经发布的语义、协议兼容承诺和迁移路径不能因排期变化而被削弱。" }
        ] },
        { title: "明确不在路线中承诺的事项", blocks: [
          { type: "list", items: ["不会为了版本号引入第二套 Pipe、Stream 或 Error 模型。", "不会在缺少兼容策略时直接公开 Runtime 内部 C 结构体。", "不会把建议时间窗口当作牺牲测试、安全或跨平台验证的理由。", "不会同时推进远程包仓库、Native ABI 和多套官方扩展而绕过阶段验收。"] }
        ] }
      ],
      en: [
        { title: "Five-release evolution overview", blocks: [
          { type: "note", text: "These dates are best-practice windows derived from dependency order, validation cost, and compatibility risk—not release-date commitments. A release enters freeze only after the previous stage passes its acceptance gate." },
          { type: "evolution-roadmap" }
        ] },
        { title: "Release lineage, timing, and acceptance gates", blocks: [
          { type: "table", columns: ["Release", "Recommended window (not committed)", "Primary delivery", "Required before the next stage"], rows: [
            ["v1.0.0 · Released", "2026-08-25", "Core language and VM semantics frozen", "Pipe, Value, Stream, Error, the core standard library, and three-platform release evidence completed"],
            ["v1.1.0 · Released", "2026-08-26", "Local process extensions and the official database extension", "Install/load integrity, synchronous Protocol 1 calls, database 0.2.0, and three-platform release evidence completed"],
            ["v1.2", "2026 Q4–2027 Q1", "Protocol completion and official Office validation", "Stream credit, cancellation, opaque-handle lifecycle, and capability paths survive large-workbook stress tests"],
            ["v1.3", "2027 Q2", "Database resource model", "Connection handles/pools, streaming queries, type mapping, and transactions have bounded resources and regression coverage on PostgreSQL and MySQL"],
            ["v1.4", "2027 Q3", "Package distribution and engineering toolchain", "Signature verification, dependency resolution, remote index, offline lock, and reproducible installation have a security review and rollback design"],
            ["v1.5", "2027 Q4–2028 Q1", "Runtime observability and long-term hardening", "Trace/profile/debug hooks, performance baselines, fuzzing, fault injection, and the compatibility matrix pass continuously"],
            ["v2.0", "2028 H2 at the earliest", "Ecosystem opening and ABI decision", "At least two real integrations prove the process protocol insufficient; otherwise retain the process protocol and do not publish a Native ABI"]
          ] }
        ] },
        { title: "Evolution principles", blocks: [
          { type: "table", columns: ["Principle", "Constraint"], rows: [["Freeze semantics first", "Stabilize Pipe, Value, Stream, Error, and cancellation semantics before broadening the ecosystem surface"], ["Usable and measurable before fast", "Every capability needs deterministic errors, resource bounds, and cross-platform tests before optimization"], ["Protocol first", "Integrate third-party capability through the Process Extension Protocol instead of inventing a second language model"], ["ABI only when justified", "Evaluate a Native ABI only after Runtime stabilization and measured need; choosing not to publish one is a valid result"]] },
          { type: "p", text: "Review the roadmap once per quarter. Only unfrozen releases may move; scheduling changes must not weaken published semantics, compatibility commitments, or migration paths." }
        ] },
        { title: "Explicit non-commitments", blocks: [
          { type: "list", items: ["No second Pipe, Stream, or Error model merely to justify a release number.", "No public exposure of internal Runtime C structures without a compatibility strategy.", "No use of recommended windows as a reason to skip testing, security, or cross-platform validation.", "No simultaneous rush into a remote registry, Native ABI, and multiple official extensions before stage gates pass."] }
        ] }
      ]
    }
  }
];

export function getChapter(slug: string): Chapter | undefined {
  return chapters.find((chapter) => chapter.slug === slug);
}

export function chapterKind(chapter: Chapter): "guide" | "project" | "reference" | "roadmap" {
  if (chapter.slug === "flowguard-project" || chapter.slug === "dataflow-etl-project" || chapter.slug === "asset-governance-project") return "project";
  if (chapter.slug === "extensions-roadmap" || chapter.slug === "language-vm-roadmap") return "roadmap";
  return chapter.slug === "syntax-reference" || chapter.slug === "standard-library" || chapter.slug === "cli-reference"
    ? "reference"
    : "guide";
}

for (const chapter of chapters) {
  const zhSections = chapter.sections.zh;
  const enSections = chapter.sections.en;
  if (zhSections.length !== enSections.length) {
    throw new Error(`bilingual section count differs in ${chapter.slug}`);
  }
  for (let index = 0; index < zhSections.length; index += 1) {
    const zhShape = zhSections[index].blocks.map((block) => block.type).join(",");
    const enShape = enSections[index].blocks.map((block) => block.type).join(",");
    if (zhShape !== enShape) {
      throw new Error(`bilingual block structure differs in ${chapter.slug} section ${index + 1}`);
    }
  }
}
