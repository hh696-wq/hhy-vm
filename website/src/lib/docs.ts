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
  | "cli-reference";

export type DocBlock =
  | { type: "p"; text: string }
  | { type: "note"; text: string }
  | { type: "code"; language: "hhy" | "sh" | "text"; code: string }
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
    slug: "cli-reference",
    order: 10,
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
