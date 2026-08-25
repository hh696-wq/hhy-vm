import type { Language } from "./i18n";

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
  | "practical-recipes"
  | "cli-reference";

export type DocBlock =
  | { type: "p"; text: string }
  | { type: "note"; text: string }
  | { type: "code"; language: "hhy" | "sh" | "text"; code: string }
  | { type: "terminal"; command: string; output: string }
  | { type: "list"; items: string[] };

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
  flow: `[1, 2, 3, 4, 5]
    |> stream
    |> map { number -> number * 2 }
    |> where { number -> number > 5 }
    |> take(2)
    |> print`,
  files: `path("./logs")
    |> files("**/*.log")
    |> where { file -> file.size > 1mib }
    |> flat_map { file -> read_lines(file.path) }
    |> where { line -> contains(line, "ERROR") }
    |> save_lines(path("errors.txt"))`,
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
  cli: `hhy run script.hhy
hhy check script.hhy
hhy fmt script.hhy
hhy run --dry-run script.hhy
hhy repl
hhy --version`
};

export const chapters: Chapter[] = [
  {
    slug: "quick-start",
    order: 1,
    title: { zh: "快速开始", en: "Quick Start" },
    summary: { zh: "安装 HHY，构建解释器，并运行第一个脚本。", en: "Install HHY, build the interpreter, and run your first script." },
    sections: {
      zh: [
        { title: "准备环境", blocks: [
          { type: "p", text: "HHY V1.0.0 正式支持 macOS arm64、Linux arm64 和 Linux x86_64。源码构建需要 C11 编译器、make、libcurl、PCRE2 与 BDWGC。" },
          { type: "code", language: "sh", code: "brew install curl pcre2 bdw-gc\ngit clone https://github.com/hh696-wq/hhy-vm.git\ncd hhy-vm\nmake\n./build/hhy --version" },
          { type: "note", text: "Linux 的依赖包名称因发行版而异，完整说明见仓库 INSTALL.md。" }
        ] },
        { title: "安装命令", blocks: [
          { type: "code", language: "sh", code: "make install PREFIX=\"$(brew --prefix)\"\nhhy --version" },
          { type: "p", text: "安装后，所有 .hhy 文件都可以通过 hhy run 执行。" }
        ] },
        { title: "第一个脚本", blocks: [
          { type: "code", language: "hhy", code: code.hello },
          { type: "code", language: "sh", code: "hhy run hello.hhy" }
        ] }
      ],
      en: [
        { title: "Prepare your environment", blocks: [
          { type: "p", text: "HHY V1.0.0 officially supports macOS arm64, Linux arm64, and Linux x86_64. A source build requires a C11 compiler, make, libcurl, PCRE2, and BDWGC." },
          { type: "code", language: "sh", code: "brew install curl pcre2 bdw-gc\ngit clone https://github.com/hh696-wq/hhy-vm.git\ncd hhy-vm\nmake\n./build/hhy --version" },
          { type: "note", text: "Package names vary across Linux distributions. See INSTALL.md for the full dependency matrix." }
        ] },
        { title: "Install the command", blocks: [
          { type: "code", language: "sh", code: "make install PREFIX=\"$(brew --prefix)\"\nhhy --version" },
          { type: "p", text: "After installation, execute any .hhy file with hhy run." }
        ] },
        { title: "Your first script", blocks: [
          { type: "code", language: "hhy", code: code.hello },
          { type: "code", language: "sh", code: "hhy run hello.hhy" }
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
        { title: "动态值模型", blocks: [
          { type: "p", text: "HHY 是动态类型语言，运行时值包括 Null、Bool、Int、Float、String、List、Map、Range、DateTime、Path、Duration、Bytes、Percent、Result、Stream 以及系统对象。" },
          { type: "list", items: ["let 声明不可重新赋值的绑定。", "let mut 声明可重新赋值的绑定。", "块、函数和模块拥有词法作用域。", "集合更新返回新值，不提供原地修改 API。"] }
        ] },
        { title: "函数与控制流", blocks: [
          { type: "code", language: "hhy", code: code.basics },
          { type: "p", text: "函数按值接收参数并支持闭包。V1.0 不提供重载、泛型或默认参数。" }
        ] }
      ],
      en: [
        { title: "Dynamic value model", blocks: [
          { type: "p", text: "HHY is dynamically typed. Runtime values include Null, Bool, Int, Float, String, List, Map, Range, DateTime, Path, Duration, Bytes, Percent, Result, Stream, and system objects." },
          { type: "list", items: ["let declares a binding that cannot be reassigned.", "let mut declares a reassignable binding.", "Blocks, functions, and modules use lexical scope.", "Collection updates return new values; there is no in-place collection API."] }
        ] },
        { title: "Functions and control flow", blocks: [
          { type: "code", language: "hhy", code: code.basics },
          { type: "p", text: "Functions receive values and support closures. V1.0 has no overloading, generics, or default parameters." }
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
        { title: "管道即语言", blocks: [
          { type: "p", text: "表达式 lhs |> f(a) 等价于把 lhs 注入为 f 的第一个参数。标准库统一按 Source → Transform → Filter → Action 组织。" },
          { type: "code", language: "hhy", code: code.flow }
        ] },
        { title: "惰性执行", blocks: [
          { type: "list", items: ["Stream 按需拉取，不预先物化全部输入。", "Stream 只能消费一次。", "take 可以安全截断有限流和无限流。", "sort、group_by、collect 等 barrier 会物化输入并受资源上限约束。"] }
        ] }
      ],
      en: [
        { title: "The pipe is the language", blocks: [
          { type: "p", text: "The expression lhs |> f(a) injects lhs as the first argument to f. The standard library consistently follows Source → Transform → Filter → Action." },
          { type: "code", language: "hhy", code: code.flow }
        ] },
        { title: "Lazy execution", blocks: [
          { type: "list", items: ["Streams pull values on demand instead of materializing all input.", "A Stream can be consumed only once.", "take safely bounds finite and infinite streams.", "Barriers such as sort, group_by, and collect materialize input under resource limits."] }
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
        { title: "文件系统是一等公民", blocks: [
          { type: "code", language: "hhy", code: code.files },
          { type: "p", text: "files 返回惰性 File Stream。Path 使用词法规范化；递归遍历可以启用 symlink，并具有循环保护。save_text 与 save_lines 使用原子替换语义。" }
        ] },
        { title: "常用能力", blocks: [
          { type: "list", items: ["read_text / read_lines", "write_text / append_text / save_text / save_lines", "files / copy / move / remove", "path_join / path_name / path_extension / path_parent"] }
        ] }
      ],
      en: [
        { title: "The filesystem is first-class", blocks: [
          { type: "code", language: "hhy", code: code.files },
          { type: "p", text: "files returns a lazy File Stream. Paths are lexically normalized; recursive walking can opt into symlinks with cycle protection. save_text and save_lines use atomic replacement semantics." }
        ] },
        { title: "Core capabilities", blocks: [
          { type: "list", items: ["read_text / read_lines", "write_text / append_text / save_text / save_lines", "files / copy / move / remove", "path_join / path_name / path_extension / path_parent"] }
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
        { title: "JSON 数据流", blocks: [
          { type: "code", language: "hhy", code: code.json },
          { type: "p", text: "JSON 解析严格拒绝重复 key、非法 Unicode surrogate 和非有限数值。Map 编码保持确定行为。" }
        ] },
        { title: "文本与 CSV", blocks: [
          { type: "list", items: ["contains、starts_with、ends_with、trim、split、join、replace、lower、upper", "PCRE2 正则字面量与命名捕获", "流式 CSV record，支持 header、delimiter 与 quote 选项", "String 始终验证 UTF-8，并支持嵌入 U+0000"] }
        ] }
      ],
      en: [
        { title: "JSON pipelines", blocks: [
          { type: "code", language: "hhy", code: code.json },
          { type: "p", text: "The strict JSON parser rejects duplicate keys, invalid Unicode surrogates, and non-finite numbers. Map encoding remains deterministic." }
        ] },
        { title: "Text and CSV", blocks: [
          { type: "list", items: ["contains, starts_with, ends_with, trim, split, join, replace, lower, and upper", "PCRE2 regex literals and named captures", "Streaming CSV records with header, delimiter, and quote options", "Strings always validate UTF-8 and may contain embedded U+0000"] }
        ] }
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
        { title: "安全运行命令", blocks: [
          { type: "code", language: "hhy", code: code.process },
          { type: "p", text: "run 默认直接传递 argv，不经过 Shell。options 支持 cwd、env、stdin、timeout 与 max_output；stdout_lines 将输出转成惰性行流。" }
        ] },
        { title: "系统数据", blocks: [
          { type: "list", items: ["processes：进程快照流", "system：操作系统、架构、CPU 与内存元数据", "env：环境变量只读视图", "shell：显式选择 Shell 语义，不与 run 混用"] }
        ] }
      ],
      en: [
        { title: "Run commands safely", blocks: [
          { type: "code", language: "hhy", code: code.process },
          { type: "p", text: "run passes argv directly and does not invoke a shell. Options include cwd, env, stdin, timeout, and max_output; stdout_lines exposes output as a lazy line stream." }
        ] },
        { title: "System data", blocks: [
          { type: "list", items: ["processes: a stream of process snapshots", "system: OS, architecture, CPU, and memory metadata", "env: a read-only view of environment variables", "shell: explicit shell semantics, separate from run"] }
        ] }
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
        { title: "请求管道", blocks: [
          { type: "code", language: "hhy", code: code.http },
          { type: "p", text: "http.get/post/put/delete 创建不可变 RequestPlan；timeout 与 retry 修改计划；send 才执行网络副作用。response_body 会验证状态码并返回文本。" }
        ] },
        { title: "V1.0 边界", blocks: [
          { type: "list", items: ["支持 query、header、body、proxy 与 redirect 选项", "支持文本和二进制响应", "响应体默认上限 16 MiB", "dry-run 只输出脱敏计划，不发送请求"] }
        ] }
      ],
      en: [
        { title: "Request pipeline", blocks: [
          { type: "code", language: "hhy", code: code.http },
          { type: "p", text: "http.get/post/put/delete create immutable RequestPlans; timeout and retry transform the plan; send performs the network effect. response_body validates status and returns text." }
        ] },
        { title: "V1.0 boundaries", blocks: [
          { type: "list", items: ["Query, header, body, proxy, and redirect options", "Text and binary responses", "16 MiB default response-body limit", "Dry-run prints a redacted plan without sending the request"] }
        ] }
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
        { title: "有界 parallel", blocks: [
          { type: "code", language: "hhy", code: code.parallel },
          { type: "p", text: "parallel(n) 最多运行 n 个隔离 worker，按输入顺序输出；一个任务失败会取消剩余任务。跨 worker 的值必须满足 Sendable 检查。" }
        ] },
        { title: "监听文件变化", blocks: [
          { type: "code", language: "hhy", code: code.watch },
          { type: "p", text: "watch 返回无限 FileEvent Stream。通过 debounce、where 和 take 等 Flow 算子组合自动化任务，并统一响应 Ctrl+C。" }
        ] }
      ],
      en: [
        { title: "Bounded parallelism", blocks: [
          { type: "code", language: "hhy", code: code.parallel },
          { type: "p", text: "parallel(n) runs at most n isolated workers and emits results in input order. One failure cancels the remaining tasks. Values crossing worker boundaries must pass Sendable validation." }
        ] },
        { title: "Watch filesystem changes", blocks: [
          { type: "code", language: "hhy", code: code.watch },
          { type: "p", text: "watch returns an infinite FileEvent Stream. Compose automation with debounce, where, take, and other Flow operators; Ctrl+C uses the same cancellation path." }
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
        { title: "Pure HHY 模块", blocks: [
          { type: "code", language: "hhy", code: code.module },
          { type: "p", text: "V1.0 支持相对 import、具名导入、别名、export、模块缓存和循环检测。不支持远程 import 或第三方包安装。" }
        ] },
        { title: "结构化错误", blocks: [
          { type: "code", language: "hhy", code: code.errors },
          { type: "p", text: "try/catch 处理即时错误，on_error 可恢复惰性 Flow 错误。所有失败沿统一 Error stage 传播，并在退出作用域时释放资源。" }
        ] }
      ],
      en: [
        { title: "Pure HHY modules", blocks: [
          { type: "code", language: "hhy", code: code.module },
          { type: "p", text: "V1.0 supports relative imports, named imports, aliases, exports, module caching, and cycle detection. Remote imports and third-party package installation are not included." }
        ] },
        { title: "Structured errors", blocks: [
          { type: "code", language: "hhy", code: code.errors },
          { type: "p", text: "try/catch handles eager errors; on_error can recover lazy Flow errors. Every failure travels through one Error stage and resources unwind when scope exits." }
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
          { type: "code", language: "hhy", code: code.hello },
          { type: "terminal", command: "hhy run examples/00-hello.hhy", output: "HHY: Flow\nHHY: Pipe\nHHY: System\n\n✓ exit 0 · Flow 管道执行完成" }
        ] },
        { title: "并发提取日志告警", blocks: [
          { type: "p", text: "递归扫描大日志文件，使用 4 个 worker 提取 ERROR/WARN 行，并将来源文件写入结果。适合服务器日志归档、故障排查和定时任务。" },
          { type: "code", language: "hhy", code: code.practicalLogs },
          { type: "code", language: "sh", code: "hhy run log-errors.hhy ./logs ./output/errors.txt" },
          { type: "terminal", command: "hhy run log-errors.hhy ./logs ./output/errors.txt && head -3 ./output/errors.txt", output: "logs/api.log: 2026-08-25T09:18:42Z ERROR database timeout after 3000ms\nlogs/worker.log: 2026-08-25T09:18:44Z WARN retrying job #1842\nlogs/api.log: 2026-08-25T09:18:47Z ERROR upstream returned 502\n\n✓ exit 0 · 3 条告警已写入 output/errors.txt" }
        ] },
        { title: "从 API 同步活跃用户", blocks: [
          { type: "p", text: "请求用户接口，经过超时、重试、JSON 解析和字段裁剪后，只把活跃用户原子写入本地文件。对应 examples/02-active-users.hhy。" },
          { type: "code", language: "hhy", code: code.practicalActiveUsers },
          { type: "code", language: "sh", code: "hhy run active-users.hhy https://api.example.com/users active-users.json" },
          { type: "terminal", command: "hhy run active-users.hhy http://127.0.0.1:9000/users active-users.json && cat active-users.json", output: "[\n  { \"id\": 101, \"name\": \"Ada\", \"email\": \"ada@example.com\" },\n  { \"id\": 108, \"name\": \"Linus\", \"email\": \"linus@example.com\" }\n]\n\n✓ exit 0 · 2 位活跃用户已写入 active-users.json" }
        ] },
        { title: "进程 CPU / 内存监控", blocks: [
          { type: "p", text: "每 5 秒读取一次进程快照，找出 CPU 超过 70% 或内存超过 1 GiB 的进程，并按内存倒序输出前 10 个。对应 examples/03-process-monitor.hhy。" },
          { type: "code", language: "hhy", code: code.practicalProcesses },
          { type: "terminal", command: "hhy run examples/03-process-monitor.hhy", output: "[{ pid: 8421, name: \"node\", cpu: 82.4%, memory: 1.42 GiB },\n { pid: 9107, name: \"hhy\",  cpu: 74.1%, memory: 86.3 MiB }]\n\nnext sample in 5s… · Ctrl+C 安全退出" }
        ] },
        { title: "批量服务健康检查", blocks: [
          { type: "p", text: "并发探测多个服务，统一设置超时与重试；单个接口失败时记录错误，不中断整批巡检。可接入发布检查或 CI。" },
          { type: "code", language: "hhy", code: code.practicalHealth },
          { type: "terminal", command: "hhy run health-check.hhy", output: "[\n  { \"name\": \"users\",   \"ok\": true,  \"status\": \"healthy\", \"error\": null },\n  { \"name\": \"orders\",  \"ok\": true,  \"status\": \"healthy\", \"error\": null },\n  { \"name\": \"billing\", \"ok\": false, \"status\": \"unreachable\", \"error\": \"request timed out\" }\n]\n\n✓ exit 0 · 3 个服务并发完成，单点失败未中断批次" }
        ] },
        { title: "业务进阶 01 · 发布质量门禁", blocks: [
          { type: "p", text: "在发布前并行执行测试、Lint 和生产构建，生成机器可读报告；任一检查失败就用稳定退出码阻止发布。适合 CI/CD、灰度发布和交付验收。" },
          { type: "code", language: "hhy", code: code.businessReleaseGate },
          { type: "terminal", command: "hhy run release-gate.hhy", output: "unit-tests       PASS  4.28s\nlint             PASS  1.14s\nproduction-build PASS  6.72s\nrelease-gate.json written\n\n✓ exit 0 · release gate passed" }
        ] },
        { title: "业务进阶 02 · 源码敏感信息审计", blocks: [
          { type: "p", text: "并发扫描配置与源码，定位疑似 API Key、密码和私钥内容，汇总成可供安全团队复核的报告。适合提交前检查和合规巡检。" },
          { type: "code", language: "hhy", code: code.businessSecretAudit },
          { type: "terminal", command: "hhy run secret-audit.hhy ./services secret-findings.txt", output: "services/billing/.env: PAYMENT_API_KEY=***\nservices/auth/config.yml: PASSWORD: ***\n\n✓ exit 0 · 2 条疑似敏感信息待复核" },
          { type: "note", text: "示例输出已脱敏。实际落地时应限制报告权限，并在流水线中避免打印秘密原文。" }
        ] },
        { title: "业务进阶 03 · 订单与支付自动对账", blocks: [
          { type: "p", text: "把订单 CSV 与支付 CSV 合并为一条数据流，按 order_id 分组，找出缺少记录或金额不一致的异常订单。适合每日财务对账。" },
          { type: "code", language: "hhy", code: code.businessReconcile },
          { type: "terminal", command: "hhy run reconcile.hhy orders.csv payments.csv exceptions.json", output: "orders: 12,480 · payments: 12,472\nmatched: 12,461\nexceptions: 19 → exceptions.json\n\n✓ exit 0 · 对账报告已原子写入" }
        ] },
        { title: "业务进阶 04 · SaaS 多租户用量快照", blocks: [
          { type: "p", text: "有界并发拉取各租户用量，统一处理超时和重试；单租户失败被隔离并记录，不影响整份快照生成。适合计费、容量分析和客户成功报表。" },
          { type: "code", language: "hhy", code: code.businessTenantSnapshot },
          { type: "terminal", command: "hhy run tenant-snapshot.hhy", output: "acme  ✓ requests=184203 storage_gb=82.4\nnova  ✓ requests=99102  storage_gb=41.8\norbit ✗ request timed out\n\n✓ exit 0 · tenant-usage-snapshot.json 包含成功数据与失败原因" }
        ] },
        { title: "业务进阶 05 · 大体积素材治理", blocks: [
          { type: "p", text: "遍历图片与视频素材，筛选超过 5 MiB 的文件并按体积倒序生成 JSON 清单，帮助内容团队定位需要压缩或迁移的资产。" },
          { type: "code", language: "hhy", code: code.businessAssetAudit },
          { type: "terminal", command: "hhy run asset-audit.hhy ./public asset-report.json", output: "scanned 1,842 assets\nlarge assets: 27\nlargest: public/video/launch.mp4 · 184.2 MiB\n\n✓ exit 0 · asset-report.json 已生成" }
        ] },
        { title: "监听源码并自动构建", blocks: [
          { type: "p", text: "监听 C 代码变化，通过 debounce 合并短时间内的连续保存，再执行 make。构建失败只打印错误，监听任务继续运行。" },
          { type: "code", language: "hhy", code: code.practicalWatch },
          { type: "code", language: "sh", code: "hhy run watch-build.hhy ./src" },
          { type: "terminal", command: "hhy run watch-build.hhy ./src", output: "watching ./src recursively…\nchanged: src/runtime/flow.c\ncc -std=c11 -O2 -c src/runtime/flow.c\ncc build/*.o -lcurl -lpcre2-8 -lgc -o build/hhy\nBuild complete: build/hhy\n\n✓ watcher remains active · waiting for the next change" }
        ] },
        { title: "从 CSV 生成部门汇总报表", blocks: [
          { type: "p", text: "读取员工 CSV，筛选在职人员，按部门统计人数和薪资总额，最后原子写入格式化 JSON。输入列为 name、department、active、salary。" },
          { type: "code", language: "hhy", code: code.practicalCsv },
          { type: "code", language: "sh", code: "hhy run csv-report.hhy employees.csv department-report.json" },
          { type: "terminal", command: "hhy run csv-report.hhy employees.csv department-report.json && cat department-report.json", output: "[\n  { \"department\": \"Engineering\", \"employees\": 12, \"total_salary\": 2160000 },\n  { \"department\": \"Product\", \"employees\": 5, \"total_salary\": 810000 }\n]\n\n✓ exit 0 · department-report.json 已原子写入" }
        ] },
        { title: "大文件备份（支持 dry-run）", blocks: [
          { type: "p", text: "找出超过 100 MiB 的文件并复制到备份目录。先用 dry-run 检查动作计划，确认无误后再真实执行。" },
          { type: "code", language: "hhy", code: code.practicalBackup },
          { type: "code", language: "sh", code: "hhy run --dry-run backup-large.hhy ./downloads ./backup\nhhy run backup-large.hhy ./downloads ./backup" },
          { type: "terminal", command: "hhy run --dry-run backup-large.hhy ./downloads ./backup", output: "copy downloads/archive.tar -> backup/archive.tar\ncopy downloads/database.dump -> backup/database.dump\n[dry-run] copy downloads/archive.tar → backup/archive.tar\n[dry-run] copy downloads/database.dump → backup/database.dump\n\n✓ exit 0 · 仅生成计划，没有写入文件" },
          { type: "note", text: "备份脚本默认不覆盖同名文件，并自动创建目标目录；正式执行前仍建议先运行 dry-run。" }
        ] },
        { title: "07 · 语言基础综合练习", blocks: [
          { type: "p", text: "用一个小型汇总任务串起变量、List、Map、函数、条件、循环、作用域和错误处理。对应 examples/07-language-basics.hhy。" },
          { type: "code", language: "hhy", code: code.basics },
          { type: "terminal", command: "hhy run examples/07-language-basics.hhy", output: "{\n  \"count\": 2,\n  \"total\": 40,\n  \"average\": 20\n}\n\n✓ exit 0 · 汇总结果已生成" }
        ] }
      ],
      en: [
        { title: "00 · Hello HHY and Flow", blocks: [
          { type: "p", text: "The smallest runnable example: turn a List into a Stream, map and filter values, then print the result. Corresponds to examples/00-hello.hhy." },
          { type: "code", language: "hhy", code: code.hello },
          { type: "terminal", command: "hhy run examples/00-hello.hhy", output: "HHY: Flow\nHHY: Pipe\nHHY: System\n\n✓ exit 0 · Flow pipeline completed" }
        ] },
        { title: "Extract log alerts concurrently", blocks: [
          { type: "p", text: "Recursively scan large log files with four workers, extract ERROR/WARN lines, and retain each source path. Useful for incident response and scheduled log jobs." },
          { type: "code", language: "hhy", code: code.practicalLogs },
          { type: "code", language: "sh", code: "hhy run log-errors.hhy ./logs ./output/errors.txt" },
          { type: "terminal", command: "hhy run log-errors.hhy ./logs ./output/errors.txt && head -3 ./output/errors.txt", output: "logs/api.log: 2026-08-25T09:18:42Z ERROR database timeout after 3000ms\nlogs/worker.log: 2026-08-25T09:18:44Z WARN retrying job #1842\nlogs/api.log: 2026-08-25T09:18:47Z ERROR upstream returned 502\n\n✓ exit 0 · 3 alerts written to output/errors.txt" }
        ] },
        { title: "Sync active users from an API", blocks: [
          { type: "p", text: "Fetch users with timeout and retry, parse JSON, select fields, and atomically save only active users. Corresponds to examples/02-active-users.hhy." },
          { type: "code", language: "hhy", code: code.practicalActiveUsers },
          { type: "code", language: "sh", code: "hhy run active-users.hhy https://api.example.com/users active-users.json" },
          { type: "terminal", command: "hhy run active-users.hhy http://127.0.0.1:9000/users active-users.json && cat active-users.json", output: "[\n  { \"id\": 101, \"name\": \"Ada\", \"email\": \"ada@example.com\" },\n  { \"id\": 108, \"name\": \"Linus\", \"email\": \"linus@example.com\" }\n]\n\n✓ exit 0 · 2 active users written to active-users.json" }
        ] },
        { title: "Monitor process CPU and memory", blocks: [
          { type: "p", text: "Sample processes every five seconds, keep CPU-heavy or memory-heavy entries, and print the top ten by memory. Corresponds to examples/03-process-monitor.hhy." },
          { type: "code", language: "hhy", code: code.practicalProcesses },
          { type: "terminal", command: "hhy run examples/03-process-monitor.hhy", output: "[{ pid: 8421, name: \"node\", cpu: 82.4%, memory: 1.42 GiB },\n { pid: 9107, name: \"hhy\",  cpu: 74.1%, memory: 86.3 MiB }]\n\nnext sample in 5s… · Ctrl+C exits safely" }
        ] },
        { title: "Check service health in batches", blocks: [
          { type: "p", text: "Probe multiple services concurrently with consistent timeouts and retries. A failed endpoint is recorded without terminating the whole batch." },
          { type: "code", language: "hhy", code: code.practicalHealth },
          { type: "terminal", command: "hhy run health-check.hhy", output: "[\n  { \"name\": \"users\",   \"ok\": true,  \"status\": \"healthy\", \"error\": null },\n  { \"name\": \"orders\",  \"ok\": true,  \"status\": \"healthy\", \"error\": null },\n  { \"name\": \"billing\", \"ok\": false, \"status\": \"unreachable\", \"error\": \"request timed out\" }\n]\n\n✓ exit 0 · all 3 checks completed despite one endpoint failure" }
        ] },
        { title: "Business 01 · Release quality gate", blocks: [
          { type: "p", text: "Run tests, lint, and production builds in parallel, save a machine-readable report, and block a release with a stable exit code when any check fails." },
          { type: "code", language: "hhy", code: code.businessReleaseGate },
          { type: "terminal", command: "hhy run release-gate.hhy", output: "unit-tests       PASS  4.28s\nlint             PASS  1.14s\nproduction-build PASS  6.72s\nrelease-gate.json written\n\n✓ exit 0 · release gate passed" }
        ] },
        { title: "Business 02 · Source secret audit", blocks: [
          { type: "p", text: "Scan configuration and source files concurrently for suspected API keys, passwords, and private keys, then produce a security review report." },
          { type: "code", language: "hhy", code: code.businessSecretAudit },
          { type: "terminal", command: "hhy run secret-audit.hhy ./services secret-findings.txt", output: "services/billing/.env: PAYMENT_API_KEY=***\nservices/auth/config.yml: PASSWORD: ***\n\n✓ exit 0 · 2 suspected secrets require review" },
          { type: "note", text: "The example output is redacted. Restrict report access and avoid printing raw secrets in production pipelines." }
        ] },
        { title: "Business 03 · Order and payment reconciliation", blocks: [
          { type: "p", text: "Merge order and payment CSV files into one flow, group records by order_id, and report missing records or mismatched amounts." },
          { type: "code", language: "hhy", code: code.businessReconcile },
          { type: "terminal", command: "hhy run reconcile.hhy orders.csv payments.csv exceptions.json", output: "orders: 12,480 · payments: 12,472\nmatched: 12,461\nexceptions: 19 → exceptions.json\n\n✓ exit 0 · reconciliation report written atomically" }
        ] },
        { title: "Business 04 · Multi-tenant usage snapshot", blocks: [
          { type: "p", text: "Fetch tenant usage with bounded concurrency, consistent retries, and failure isolation. Useful for billing, capacity analysis, and customer success reports." },
          { type: "code", language: "hhy", code: code.businessTenantSnapshot },
          { type: "terminal", command: "hhy run tenant-snapshot.hhy", output: "acme  ✓ requests=184203 storage_gb=82.4\nnova  ✓ requests=99102  storage_gb=41.8\norbit ✗ request timed out\n\n✓ exit 0 · snapshot contains both data and failure reasons" }
        ] },
        { title: "Business 05 · Oversized asset governance", blocks: [
          { type: "p", text: "Find large image and video assets, sort them by size, and produce a JSON inventory for compression or storage migration work." },
          { type: "code", language: "hhy", code: code.businessAssetAudit },
          { type: "terminal", command: "hhy run asset-audit.hhy ./public asset-report.json", output: "scanned 1,842 assets\nlarge assets: 27\nlargest: public/video/launch.mp4 · 184.2 MiB\n\n✓ exit 0 · asset-report.json generated" }
        ] },
        { title: "Watch sources and rebuild", blocks: [
          { type: "p", text: "Watch C sources, debounce rapid saves, and run make. Build failures are reported while the watcher stays alive." },
          { type: "code", language: "hhy", code: code.practicalWatch },
          { type: "code", language: "sh", code: "hhy run watch-build.hhy ./src" },
          { type: "terminal", command: "hhy run watch-build.hhy ./src", output: "watching ./src recursively…\nchanged: src/runtime/flow.c\ncc -std=c11 -O2 -c src/runtime/flow.c\ncc build/*.o -lcurl -lpcre2-8 -lgc -o build/hhy\nBuild complete: build/hhy\n\n✓ watcher remains active · waiting for the next change" }
        ] },
        { title: "Build a department report from CSV", blocks: [
          { type: "p", text: "Read employee CSV records, keep active employees, aggregate headcount and salary by department, and atomically save formatted JSON." },
          { type: "code", language: "hhy", code: code.practicalCsv },
          { type: "code", language: "sh", code: "hhy run csv-report.hhy employees.csv department-report.json" },
          { type: "terminal", command: "hhy run csv-report.hhy employees.csv department-report.json && cat department-report.json", output: "[\n  { \"department\": \"Engineering\", \"employees\": 12, \"total_salary\": 2160000 },\n  { \"department\": \"Product\", \"employees\": 5, \"total_salary\": 810000 }\n]\n\n✓ exit 0 · department-report.json written atomically" }
        ] },
        { title: "Back up large files with dry-run", blocks: [
          { type: "p", text: "Find files over 100 MiB and copy them into a backup directory. Inspect the plan with dry-run before performing real writes." },
          { type: "code", language: "hhy", code: code.practicalBackup },
          { type: "code", language: "sh", code: "hhy run --dry-run backup-large.hhy ./downloads ./backup\nhhy run backup-large.hhy ./downloads ./backup" },
          { type: "terminal", command: "hhy run --dry-run backup-large.hhy ./downloads ./backup", output: "copy downloads/archive.tar -> backup/archive.tar\ncopy downloads/database.dump -> backup/database.dump\n[dry-run] copy downloads/archive.tar → backup/archive.tar\n[dry-run] copy downloads/database.dump → backup/database.dump\n\n✓ exit 0 · plan generated without writing files" },
          { type: "note", text: "The recipe refuses to overwrite files and creates parent directories, but you should still inspect the dry-run plan first." }
        ] },
        { title: "07 · Language basics in one task", blocks: [
          { type: "p", text: "A small aggregation task combining variables, Lists, Maps, functions, conditions, loops, scopes, and error handling. Corresponds to examples/07-language-basics.hhy." },
          { type: "code", language: "hhy", code: code.basics },
          { type: "terminal", command: "hhy run examples/07-language-basics.hhy", output: "{\n  \"count\": 2,\n  \"total\": 40,\n  \"average\": 20\n}\n\n✓ exit 0 · summary generated" }
        ] }
      ]
    }
  },
  {
    slug: "cli-reference",
    order: 11,
    title: { zh: "CLI 参考", en: "CLI Reference" },
    summary: { zh: "运行、检查、格式化、REPL 与 dry-run。", en: "Run, check, format, use the REPL, and inspect dry-run plans." },
    sections: {
      zh: [
        { title: "命令", blocks: [
          { type: "code", language: "sh", code: code.cli },
          { type: "list", items: ["run：执行脚本并传递 args", "check：Parser 与 Checker 静态检查", "fmt：规范化源码格式", "ast / tokens：检查编译器中间结构", "dry-run：拦截文件写入、进程启动和网络发送并输出脱敏计划"] }
        ] },
        { title: "稳定退出码", blocks: [
          { type: "p", text: "V1.0 规范化退出码 0–5，分别覆盖成功、运行时失败、语法/检查失败、用法错误、资源限制与内部错误。自动化脚本应按退出码而不是错误文本进行分支。" }
        ] }
      ],
      en: [
        { title: "Commands", blocks: [
          { type: "code", language: "sh", code: code.cli },
          { type: "list", items: ["run: execute a script and pass args", "check: static Parser and Checker validation", "fmt: normalize source formatting", "ast / tokens: inspect compiler structures", "dry-run: intercept file writes, process launches, and network sends, then print a redacted plan"] }
        ] },
        { title: "Stable exit codes", blocks: [
          { type: "p", text: "V1.0 defines exit codes 0–5 for success, runtime failure, syntax/check failure, usage error, resource limit, and internal error. Automation should branch on exit codes rather than error text." }
        ] }
      ]
    }
  }
];

export function getChapter(slug: string): Chapter | undefined {
  return chapters.find((chapter) => chapter.slug === slug);
}
