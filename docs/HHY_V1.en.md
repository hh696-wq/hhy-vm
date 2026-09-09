# HHY Language v1.0 Unified Specification

<!-- source-sha256: 49de99925b3e335207841aa445648f682d9536ea7d105bb7e6203e3c36d5b807 -->

> Current language specification: `1.0.0` (frozen); current compatible implementation: `1.7.0`
> Specification status: v1.0 frozen
> Website: [hhylang.dev](https://hhylang.dev)
> Positioning: Flow-first system scripting language
> Tagline: Pipe Everything.

This is the English translation of the HHY v1.0 specification. The [Chinese original](HHY_V1.md) is the authoritative source for syntax, runtime, standard library, CLI, and tests. If an experimental implementation conflicts with the specification, update the specification and record the decision before changing code.

## Current Compatible Implementation Addendum · v1.7.0

This section records compatible additions after the v1.0 freeze; it does not change the v1.0 core semantics below.
v1.4.0–v1.4.2 are Web Runtime capability milestones, released together through v1.4.3.

| Area | v1.7.0 status |
|---|---|
| Execution engine | Bytecode by default; AST permanently retained as the semantic oracle and fallback through `--engine ast` / `HHY_ENGINE=ast` |
| C Embedding | Opaque `HhyApplication` / `HhyContext` handles, JSON ABI, one-time application loading and repeatable `hhy_call`; Runtime internals are not exposed |
| Web Runtime | `hhy serve`, HTTP/1.1, Router, Query/Header/Cookie, Middleware, static files, uploads, CORS, signed cookies, gzip, and development hot reload |
| Streaming and operations | Stream, SSE, Range, multiple Workers, health checks, structured logs, and Prometheus metrics |
| Service boundaries | TLS and HTTP/2 are handled by a reverse proxy or load balancer; WebSocket is outside the v1.4 scope |
| Historical v1.4.3 Web validation | 100,000 repeated calls on one loaded Context with dual-engine output comparison; 1,000,000 loopback HTTP requests at concurrency 16 with zero failures |
| Database 1.0.0 | Bounded MySQL/PostgreSQL connection pools, configuration Maps, explicit remote endpoint authorization, and TLS identity verification |
| DB transactions and results | Read/write transactions, transaction callbacks, savepoints, reusable prepared statements, atomic batches, incremental cursors, HHY Stream, column metadata, and exact types |
| DB host integration | Duration-to-millisecond conversion, BytesBuffer round trips, request-end cleanup, cancellation propagation, extension crash recovery, and Worker isolation after fork |
| v1.5.0 release validation | Local/CI tests with both databases and AST/Bytecode, positive and negative TLS cases, sanitizers, CMS installation database fixtures, and cross-platform release checks passed |
| Validation boundaries | DB releases support macOS arm64 and Linux arm64/x86_64; Windows Runtime packages exclude DB; no claim is made that real RDS testing or a 24-hour soak passed |

### v1.7.0 Optimizing Compiler

Structured HIR, independent CFG/verifier, six switchable passes, integer register MIR, parameter feedback guards/deoptimization and local List scalar replacement are implemented. Enable them explicitly with `HHY_COMPILER=ir`, `HHY_FEEDBACK_SPECIALIZATION=1` and `HHY_SCALAR_REPLACEMENT=1`; direct Bytecode remains the default. Scalar replacement preserves original GC/quota allocation reservations and does not claim physical heap allocation elimination. Four-platform CI and release validation pass; real workloads have not met the default-enablement benefit gate. Language semantics, Process Extension Protocol 1 and independently versioned extensions remain compatible.

See [Compiler IR](https://github.com/hh696-wq/hhy-vm/blob/main/docs/architecture/COMPILER_IR.md) for implementation limits and [v1.7.0](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.7.0) for the release.

### Database 1.0.0 Compatibility Contract

- HHY 1.5.0 is the minimum version. Remote connections use a trusted configuration Map containing driver, host, port, user, password, database, allow, tls, and optional ca; legacy URLs retain loopback support.
- Pools are isolated by data source and extension process; defaults are max_open=4 and max_idle=2. Multiple Workers require a combined connection budget.
- query preserves String/null results by default; typed mode enables type mapping. Decimal and large integers use exact wrappers. Fields, parameters, results, messages, and leases are bounded.
- with_transaction commits when the callback succeeds and rolls back when it fails. Handles cannot cross requests or Workers. Writes must not be replayed automatically when the commit outcome is unknown.
- DB Stream uses host-driven incremental cursor fetching. Protocol 1 negotiates scoped_resources and scope-end/cancellation messages; this does not expose generic Stream credit or a Native ABI.
- The install.hhy acceptance case is a minimal database lifecycle fixture, not a complete corporate website CMS. Applications must still record migration checkpoints for multi-step MySQL DDL.

See the [Database 1.0.0 README](https://github.com/hh696-wq/hhy-vm/blob/v1.5.0/extensions/database/README.md) for the full API and boundaries,
the [DB acceptance record](https://github.com/hh696-wq/hhy-vm/blob/v1.5.0/extensions/database/ACCEPTANCE.md) for measurements and reproduction,
and [HHY 1.5.0](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.5.0) for the release record.

For Web Runtime APIs, usage, and deployment boundaries, see
[Web Runtime](WEB_RUNTIME.md); for measurement conditions and the release record, see
the [v1.4.3 release notes](https://github.com/hh696-wq/hhy-vm/releases/tag/v1.4.3). These records do not present single-machine throughput as a general performance guarantee.

## 1. Product Definition

HHY is a system scripting language centered on data flow. Its unified pipeline model connects files, processes, networks, and structured data, making system automation as straightforward as describing data flow.

```text
HHY v1.0 = Complete dynamic scripting language + Flow execution model + System standard library
```

```hhy
path("./logs")
    |> files("**/*.log")
    |> where { file -> file.size > 1mb }
    |> flat_map { file -> read_lines(file.path) }
    |> where { line -> contains(line, "ERROR") }
    |> take(100)
    |> save_lines(path("errors.txt"))
```

HHY is not a natural-language executor, does not depend on AI, and is not an alternative spelling of Shell commands. Every source file has deterministic tokens, AST, scopes, execution plans, and error behavior.

## 2. Language Principles

- **Flow**: Files, lines, processes, HTTP responses, and system events share a unified Stream model.
- **Pipe**: `|>` is the core composition mechanism, not an add-on syntax convenience.
- **System**: Path, File, Process, HttpRequest, and FileEvent are first-class values.
- **Simple**: One calling syntax, explicit closures, dynamic types, and clear errors.
- **Native Units**: Bytes, Duration, and Percent are native values.
- **Lazy where useful**: Streams execute lazily; ordinary scalar expressions execute immediately.
- **Explicit effects**: Reading, writing, launching processes, and sending network requests have explicit operators.
- **Safe by default**: Commands bypass the Shell by default, TLS verification is enabled by default, and resources are bounded by default.

Brand expression:

```text
HHY
Pipe Everything.
A flow-first scripting language for system automation.
Built solo. Designed to flow.
```

## 3. Frozen v1.0 Decisions

The following decisions are the implementation baseline. Changes require updating this document first.

1. HHY uses dynamic types and runtime checks.
2. Variables are immutable by default; mutable bindings use `let mut`.
3. Ordinary calls consistently use `name(args)`.
4. `value |> fn(a)` is equivalent to `fn(value, a)`.
5. A pipeline stage without additional arguments may use `value |> fn`, equivalent to `fn(value)`.
6. Per-item operators such as `where` accept explicit closures only, without implicit field scopes.
7. The full closure form is `{ param -> expression }` or `{ param -> statements }`.
8. Single-parameter closures may use the implicit `it` shorthand `{ expression }`.
9. Maps use `{ key: value }`; code blocks are distinguished by syntactic context.
10. `map` does not automatically flatten nested Streams; use `flat_map` when flattening is needed.
11. Streams are lazy, pull-based, and single-consumption by default.
12. Ordinary errors fail fast by default; per-item errors must be explicitly converted to Result to continue.
13. HTTP first constructs an HttpRequest; only `send` causes the side effect.
14. `run` accepts an argument array and bypasses the Shell; Shell mode requires explicit `shell`.
15. Parallel preserves order, limits concurrency, bounds buffering, and fails fast by default.
16. The C Runtime uses bounded conservative tracing GC for the language heap; system resources are closed explicitly and must not rely on GC finalizers.
17. Values crossing Parallel workers use frozen snapshots; mutable objects are not shared.
18. v1.0 reserves extension naming, type, and Flow contract boundaries, but does not load third-party extensions or freeze a public Native ABI.
19. v1.0 officially supports macOS arm64 and Linux x86_64/arm64; Windows is a later target.

## 4. Source Files and Lexical Rules

- The file extension is `.hhy`.
- Source text is UTF-8; LF and CRLF are accepted.
- Identifiers and keywords are case-sensitive.
- Identifiers use ASCII letters, digits, and `_`, and cannot start with a digit.
- `{ ... }` denotes a block or Map according to its syntactic position.
- A newline ends a statement by default; optional `;` is allowed.
- Newlines do not end statements inside unclosed parentheses, brackets, or braces.
- `|>` at the end of a line or at the start of the next significant line continues the current pipeline.
- `#` starts a line comment; a shebang is allowed on the first line.
- Tabs and spaces do not determine semantics.
- String literals support `\\`, `\"`, `\n`, `\r`, `\t`, `\b`, `\f`, and `\0`; other backslash escapes are invalid. Write Unicode characters directly as UTF-8. v1.0 does not define `\u` source escapes (`\uXXXX` inside JSON still follows the JSON specification).

```hhy
#!/usr/bin/env hhy

# first HHY program
let name = "HHY"
name |> print
```

### 4.1 Disambiguating Regex and Division

`/` starts a Regex where the beginning of an expression is expected; after an existing left operand it denotes division.

```hhy-snippet
let pattern = /ERROR|WARN/i
let ratio = total / count
```

The Lexer must retain token line/column positions and original slices; Parser errors must show source excerpts.

## 5. Literals and Operators

Literals:

```text
null true false
42 -10 0xff 0b1010 1.5 1e6
"hello" "Hello, {name}"
/ERROR|WARN/i
[1, 2, 3]
{ name: "Tom", age: 20 }
1..10
10mb 5s 80%
```

Operator precedence, highest to lowest:

```text
() [] .
not  unary- unary+
* / %
+ -
< <= > >=
== !=
and
or
??
|>
=
```

Rules:

- `and`, `or`, and `??` short-circuit.
- `=` may assign only to `let mut` bindings.
- `|>` is left-associative.
- `%` immediately following a number without whitespace belongs to Percent; in a binary position it denotes modulo.
- Range `a..b` includes the start and excludes the end.

## 6. Calls and Pipe Semantics

### 6.1 Ordinary Calls

All ordinary function and standard library calls use parentheses:

```hhy
print("hello")
read_text(path("hello.txt"))
copy(path("a.txt"), path("backup/a.txt"))
```

Special forms such as `copy "a" to "b"` are not used, avoiding separate Parser grammar for each standard library function.

### 6.2 Pipe Lowering Rules

```text
x |> f           => f(x)
x |> f(a, b)     => f(x, a, b)
x |> obj.f(a)    => obj.f(x, a)
```

Examples:

```hhy
"hello" |> upper |> print
path("./src") |> files("*.c") |> take(10) |> print
```

The right-hand side must be a callable value or call expression. Values cannot be injected automatically into arbitrary subexpressions.

### 6.3 What Pipe Does Not Do Automatically

- Convert a single value into a Stream.
- Flatten `Stream<Stream<T>>`.
- Access fields of `it`.
- Ignore errors.
- Stringify structured values.
- Execute the Shell.

## 7. Dynamic Value Model

Core types:

```text
Null Bool Int Float String Regex BytesBuffer
List Map Range Function Error Result Stream
Bytes Duration Percent DateTime Path
File Directory FileEvent Process CommandResult
HttpRequest HttpResponse
```

`type()` returns the logical type names above. Result, File, Directory, FileEvent, Process, and CommandResult must not masquerade as Maps. They expose read-only fields and can be copied across Parallel workers when all fields are sendable. Under the JSON rules, system objects still cannot be encoded directly as JSON.

### 7.1 Bool and Truth Values

Conditions must be Bool. HHY does not implicitly treat an empty string, 0, an empty List, or null as false.

```hhy-snippet
if value != null { ... }
```

### 7.2 Numbers

- Int is a signed 64-bit integer.
- Int overflow raises ValueError; it neither wraps nor silently converts to Float.
- Mixed Int/Float arithmetic converts Int to Float.
- Division by zero raises ValueError.
- Float uses IEEE 754 double precision.
- JSON rejects serialization of NaN and Infinity by default.
- `1 == 1.0` is true; other types are not implicitly converted for comparison.

### 7.3 String

- String is an immutable UTF-8 byte sequence, validated at creation.
- String has an explicit byte length rather than C `\0` termination semantics. Valid `U+0000` must be fully preserved in indexing, comparison, Map keys, JSON, text operators, and file I/O.
- `byte_length` returns the byte count.
- `length` returns the Unicode code point count.
- v1.0 does not promise grapheme cluster indexing.
- String indexing returns a single-character String for the corresponding Unicode code point.
- `Bytes` is a dimensional numeric value for file or memory size; `BytesBuffer` is immutable binary
  data. Binary content must use BytesBuffer, not String; the two must not be mixed.
- Text boundaries passed to host C APIs, including Path, process argv/cwd/env, and HTTP URL/proxy/header, reject `U+0000` with ValueError. HTTP and process text results must pass UTF-8 validation; arbitrary binary responses use BytesBuffer APIs.

### 7.4 Equality and Hashing

- Null, Bool, numbers, String, Units, and Path compare by value.
- List and Map use deep comparison with recursion-depth checks.
- Function, Stream, File, Process, Request, and Response do not support value equality.
- Map keys must be String.
- `distinct/group` accept only hashable scalar keys; unsupported keys raise TypeError.

### 7.5 Type Checking

```hhy-snippet
type(value)
is_type(value, "String")
```

HHY does not perform unsafe implicit String/Number or String/Bool conversions.

## 8. Variables, Mutability, and Scope

```hhy
let name = "HHY"
let mut count = 0
count = count + 1
```

- Scoping is block-level and lexical.
- Variables must be declared before use.
- Duplicate declarations in the same scope are forbidden.
- Inner scopes may shadow outer names; the Checker emits a warning.
- `let` bindings cannot be reassigned.
- `let mut` permits reassignment.
- List and Map are immutable by default; update operations return new values.
- v1.0 has no in-place collection mutation APIs, reducing reference cycles and concurrent sharing issues.
- Closures capture the current value of a binding. Capturing `let mut` captures a shared Cell, but makes the closure unsendable to Parallel workers.

## 9. Conditionals and Loops

```hhy-snippet
if enabled {
    print("enabled")
} else if pending {
    print("pending")
} else {
    print("disabled")
}

for file in file_list {
    print(file.path)
}

let mut n = 0
while n < 10 {
    n = n + 1
}
```

v1.0 supports:

- `if / else if / else`.
- `for item in iterable`.
- `while condition`.
- `break` and `continue`.
- Iteration over List, Map entries, Range, and Stream.

`for` consumes a Stream. A consumed Stream cannot be iterated again.

## 10. Functions and Closures

```hhy
fn add(a, b) {
    return a + b
}
```

- Functions use positional parameters.
- Without an explicit `return`, the return value is null.
- Recursion, function values, and lexical closures are supported.
- Argument count is checked at call time.
- v1.0 does not support overloading, generics, or default parameters.

Full closures:

```hhy-snippet
users |> where { user -> user.active }
users |> map { user -> { name: user.name, email: user.email } }
```

Implicit single-parameter shorthand:

```hhy-snippet
numbers |> map { it * 2 }
```

A closure containing multiple statements must declare its parameter explicitly and use `return`:

```hhy-snippet
numbers |> map { number ->
    let doubled = number * 2
    return doubled
}
```

## 11. Disambiguating Maps and Code Blocks

- `{ key: value }` is a Map in expression position.
- `{ ... }` after control statements, functions, and catch is a code block.
- `{ param -> ... }` immediately following a Flow operator is a closure.
- `{ it * 2 }` is allowed as an implicit closure only where a closure argument is explicitly expected.
- Empty `{}` is an empty Map in expression position; empty blocks occur only where the grammar requires a block.

The Parser uses the parent syntax node to distinguish Map from Block; it does not guess from newlines or field names.

## 12. List, Map, Range, and DateTime

List:

- `length`, `get`, `append`, `remove_at`, `contains`.
- Indexing starts at 0; negative indices are unsupported.
- Out-of-bounds access raises IndexError.
- Modification operations return a new List.

Map:

- Keys must be unique Strings and preserve insertion order. The Checker rejects duplicate Map literal keys; duplicate JSON Object keys raise ValueError. Ambiguous first-wins/last-wins rules are not used.
- Keys retain independent byte lengths, so valid UTF-8 keys may contain `U+0000`. Lookup, update, removal, equality, and JSON encoding/decoding use the entire byte sequence.
- `map.key` and `map["key"]` are supported.
- Ordinary missing fields return null.
- Key existence differs from a null value. `require(map, key)` raises KeyError only for an absent key, and returns null when the key exists with a null value. `pick` also preserves such fields.
- `put/remove_key` return a new Map.

Range:

- `a..b` includes a and excludes b.
- v1.0 Range accepts only Int.
- Range is directly iterable without preallocating a List.

DateTime:

- Ambiguous date literals are not provided.
- `datetime.parse(text, format, timezone)` parses explicitly.
- `now()` returns a timezone-aware DateTime.
- Serialization uses RFC 3339 by default.
- Duration can be added to or subtracted from DateTime.

## 13. Stream Model

### 13.1 Basic Properties

- A Stream is a lazy, pull-based, single-consumption sequence of data.
- Upstream produces the next item only when downstream calls `next()`.
- Finite streams, infinite streams, and event streams share the same interface.
- Streams cannot be copied, compared, or serialized as JSON.
- `collect()` explicitly materializes a finite Stream into a List.
- Completion, early stopping, errors, and cancellation all call `close()`.

### 13.2 Operator Lifecycle

```text
open -> next* -> close
```

`close` must be idempotent and propagate from downstream to upstream.

### 13.3 Operator Categories

Per-item streaming:

```text
map flat_map where take skip inspect
```

Stateful but bounded:

```text
distinct debounce parallel
```

Barriers or terminals:

```text
sort group collect reduce count save
```

An infinite stream entering `sort/group/collect` must have an explicit boundary, such as a preceding `take` or time window; otherwise it raises PlanError.

### 13.4 Map and Flat Map

```text
map       Stream<T> + (T -> U)         -> Stream<U>
flat_map  Stream<T> + (T -> Stream<U>) -> Stream<U>
```

`map` never automatically flattens a returned List or Stream.

### 13.5 Unified Cancellation

The Runtime creates a CancellationToken for each execution. Ctrl+C, timeout, unhandled errors, and explicit `cancel` trigger the same cancellation path.

All file-reading, HTTP, watch, sleep, parallel, and subprocess operators must check this token periodically.

## 14. Standard Flow Operators

Sources:

```text
files read_lines processes watch stdin_lines range
```

Transformations:

```text
stream map flat_map for_each split trim replace lower upper
parse_json encode_json parse_csv encode_csv get pick
stdout_lines response_body response_bytes send_to
```

Filters:

```text
where contains starts_with ends_with regex_match distinct
```

Collections and aggregation:

```text
take skip first last count sort_by group_by reduce
sum min max any all collect
```

Actions:

```text
print print_error save_text save_lines write_bytes copy move remove send run
```

Control:

```text
parallel retry timeout on_error attempt debounce
```

Standard library names use snake_case. The standard library reference must document each operator's input, output, laziness, side effects, error types, and resource limits.

General scalar and collection functions:

```text
length type is_type to_int to_float
path path_join get require put remove_key
```

- `group_by` produces `Stream<Group>`. Group contains `key` and `values`; `values` is a finite List.
- `length` accepts String, List, or Map; Streams use the terminal operator `count`.
- `to_int/to_float` are explicit conversions; conversion failure raises ValueError.
- `stream` accepts List, Map entries, or Range and returns a single-consumption Stream.
- `reduce(initial) { state -> ... }` is a terminal operator; `state` is an immutable Map containing `acc`, `item`,
  and a zero-based `index`. The closure's return value becomes `acc` for the next item.
- `print(Stream)` / `print_error(Stream)` pull items one at a time and output one line per item without materializing the whole stream. To output a List, explicitly use `collect |> print`.

## 15. Native Units

```text
Bytes     b kb mb gb tb kib mib gib tib
Duration  ns us ms s min h d
Percent   %
```

- `kb/mb/gb` are decimal; `kib/mib/gib` are binary.
- Internally, fixed base units and overflow checks are used.
- Values with the same dimension can be compared, added, and subtracted.
- Bytes and Duration are not comparable.
- `Percent` is represented internally as double and is not restricted to 0% through 100%.
- `sleep(500ms)`, `timeout(5s)`, and File.size use the same native unit type system.

## 16. Path, Files, and Directories

String does not implicitly become Path. Use `path(text)` to construct and normalize a Path.

```hhy
path("./src")
    |> files("**/*.c", { follow_symlinks: false })
    |> where { file -> file.size > 10kb }
    |> sort_by({ order: "desc" }) { file -> file.size }
    |> print
```

File fields:

```text
path name extension size created modified
is_file is_dir is_symlink
```

`files(pattern, options)` produces File for matching regular files/symlinks and Directory for matching directories; the traversal root itself is excluded. Both share the read-only fields above.

`modified` is DateTime; `created` is `DateTime | Null`. If the OS or filesystem cannot provide a reliable creation time, it must return Null rather than fabricate one from the modification time.

Basic APIs:

```hhy
read_text(path("hello.txt"))
read_lines(path("app.log"))
read_bytes(path("image.png"))
write_text(path("hello.txt"), "Hello HHY")
append_text(path("app.log"), "done\n")
copy(path("a.txt"), path("backup/a.txt"))
move(path("a.txt"), path("data/a.txt"))
remove(path("temp.txt"))
```

Rules:

- `*`, `?`, and `**` globs are supported.
- Relative Paths resolve against the process's initial working directory and do not change with imported files.
- `path(text)` performs lexical normalization: it collapses repeated separators and `.`, and resolves removable `..`, without accessing the filesystem or resolving symlinks.
- Path exposes read-only members `name`, `extension`, and `parent`. `extension` includes the leading `.` and is an empty string when absent.
- Directory symlinks are not followed by default. `files(..., { follow_symlinks: true })` enables traversal, skipping detected directory cycles.
- Text defaults to UTF-8; invalid UTF-8 raises EncodingError.
- `read_lines` reads incrementally and removes line terminators.
- `save_lines` writes platform-independent `\n` after each element.
- `save_text` accepts only String or Stream<String>; it does not implicitly encode Map/List.
- `save_text(Stream)` and `save_lines(Stream)` pull upstream directly into a temporary file, without first
  calling `collect` or concatenating the entire output, so memory usage does not grow linearly with output file size.
- `write_text/save_*` replace atomically by default: write a temporary file in the same directory, then rename it.
- `write_text`, `write_bytes`, `save_text`, and `save_lines` accept an optional options Map:
  `overwrite` defaults to `true`; `create_parents` defaults to `false`. With
  `overwrite: false`, commit uses the platform's atomic no-replace rename. An existing or concurrently created destination raises IoError; check-then-rename races must not overwrite it. `create_parents: true` creates missing directories recursively.
- `--dry-run` intercepts copy, move, remove, write, and save.

## 17. Text and Regex

```hhy
read_lines(path("app.log"))
    |> where { line -> contains(line, "ERROR") }
    |> map { line -> replace(line, "ERROR", "ERR") }
    |> print
```

v1.0 provides:

```text
split join trim trim_start trim_end
contains starts_with ends_with replace
lower upper regex_match regex_captures
```

- Text functions operate on Unicode code points.
- Regex literals support the `i`, `m`, `s`, and `u` flags.
- The Regex engine must provide execution limits or guarantee linear time to prevent resource exhaustion.
- `regex_captures` returns the full match and byte positions; numbered captures are in `groups: List`, and named captures in `named: Map`.
- v1.0 uses the PCRE2 8-bit backend with limits on match steps, depth, heap, input size, pattern size, and capture-group count.

## 18. JSON and CSV

JSON:

```hhy
read_text(path("users.json"))
    |> parse_json
    |> get("users")
    |> where { user -> user.active }
    |> map { user -> { name: user.name, email: user.email } }
    |> encode_json({ pretty: true })
    |> print
```

- Object maps to Map; Array maps to List.
- JSON Number maps to Int or Float according to its representation.
- JSON parse errors include input line and column positions.
- Maximum size, nesting depth, and string length are bounded by resource configuration.
- NaN, Infinity, Function, Stream, and system objects cannot be encoded as JSON.

CSV:

- `parse_csv` accepts String or Stream<String> and returns `Stream<Map>`.
- `encode_csv` accepts `Stream<Map>` and returns `Stream<String>`.
- Options include `header: Bool`, a single-character `delimiter`, and a single-character `quote`.
- Each stream element from `encode_csv` is a CSV record without a line terminator; `save_lines` writes `\n`. v1.0 therefore has no `newline` encoding option that could cause double line breaks.
- Processing remains streaming and does not require loading the entire file.

## 19. Processes and Commands

```hhy
processes
    |> where { process -> process.cpu > 50% }
    |> sort_by({ order: "desc" }) { process -> process.cpu }
    |> take(10)
    |> print
```

Process fields:

```text
pid name cpu memory status command
```

Command execution:

```hhy
run(["git", "status"])

run(["git", "log", "--oneline"])
    |> stdout_lines
    |> take(10)
    |> print
```

`run(args, options)` returns CommandResult:

```text
exit_code stdout stderr duration
```

- By default, it waits for command completion and limits output size.
- `stdout_lines` produces Stream<String> from CommandResult.stdout.
- Options support cwd, env, stdin, timeout, and maximum output.
- timeout/Ctrl+C first requests graceful termination, then forces termination after a grace period.
- `shell(command)` is an explicitly dangerous API and produces a Checker notice.
- `--dry-run` does not execute run or shell; it only outputs the plan.

The `order` in `sort_by(options) { item -> key }` must be `"asc"` or `"desc"`, defaulting to `"asc"`. Sorting must be stable; incomparable keys raise TypeError.

## 20. Environment, Arguments, and System Information

```hhy
env.PATH
env["HOME"]
args

system.os
system.arch
system.hostname
system.cpu
system.memory
system.cwd
system.temp
```

- `args` is a `List<String>` excluding the script path.
- CLI arguments and host environment variables must pass UTF-8 validation before entering String/Map. Invalid host bytes raise EncodingError and must not create values that violate String invariants.
- `exit(code)` terminates the script immediately and cleans up resources.
- Missing environment variables return null; `require_env(name)` raises KeyError when absent.
- Environment overrides are passed only to child processes and do not modify the parent environment.
- `stdin_lines` is a single-consumption Stream<String>.

## 21. HTTP Request Plans

HTTP uses an explicit Request -> Policy -> Send -> Response model:

```hhy
http.get("https://api.example.com/users")
    |> timeout(5s)
    |> retry({ count: 3, backoff: 200ms })
    |> send
    |> response_body
    |> parse_json
    |> get("users")
    |> where { user -> user.active }
    |> print
```

Semantics:

- `http.get/post/put/delete` only construct HttpRequest; they do not send network requests.
- `timeout` and `retry` modify the request execution plan.
- `send(HttpRequest)` is the actual network side effect and returns HttpResponse.
- `send_to(HttpRequest, Path)` streams the response body directly to a temporary file in the destination directory and publishes it atomically on success. Its HttpResponse exposes `path` and `size` without retaining the full body.
- `response_body` validates the status and returns String; unsuccessful statuses raise HttpStatusError by default.
- `response_bytes` returns BytesBuffer.
- By default, retry applies only to connection errors, timeouts, 429, and selected 5xx statuses.
- GET/PUT/DELETE may be retried according to policy; POST is not automatically retried by default.
- TLS verification is enabled by default.
- Query parameters, headers, body, proxies, redirects, and response-size limits are supported.
- Sensitive headers such as Authorization and Cookie are excluded from ordinary logs and Error displays.
- `--dry-run` sends no requests; it outputs only the method, redacted URL, and policy.

## 22. Watch and Timers

```hhy
watch(path("./src"), { recursive: true })
    |> where { event -> event.path.extension == ".c" }
    |> debounce(200ms)
    |> for_each { event -> run(["make"]) }
```

FileEvent:

```text
kind path old_path timestamp
```

- kind is created, modified, removed, or renamed.
- old_path is present only for renamed events.
- macOS uses kqueue and Linux uses inotify. Platform events only wake the Runtime, which normalizes fields and semantics through snapshot differences.
- Recursive watchers register subdirectories and are bounded by `max_open_files`; newly added directories refresh the registration set.
- Internal `.hhy-tmp-*` files from HHY atomic write transactions do not leak as user FileEvents.
- Rename pairs are matched through stable file identifiers into a single `renamed` event with `old_path`; debounce may coalesce repeated events in a short interval.
- `debounce(window)` uses leading-edge semantics: for the same scalar value, or FileEvents with the same `kind + path`,
  the first item is emitted immediately; duplicates within the window are coalesced and restart the timer from the last duplicate.
  Different event keys do not block each other. This avoids hidden concurrency or unbounded trailing-item buffers in infinite event streams.
- Watch is an infinite Stream and must close through cancellation, timeout, or process termination.

Timers:

```hhy
every(5s)
    |> flat_map { tick -> processes }
    |> where { process -> process.cpu > 80% }
    |> print
```

`every` returns an infinite Stream<Tick>. If downstream is still processing, backpressure applies by default; processing of the same item does not overlap.

## 23. Parallel

```hhy-snippet
urls
    |> parallel(8) { url ->
        http.get(url)
            |> timeout(5s)
            |> send
            |> response_body
    }
    |> print
```

Rules:

- The argument is the maximum concurrent task count, must be greater than 0, and is subject to the global limit.
- It returns `Stream<U>` with concurrent-map semantics and no automatic flattening.
- Output preserves input order by default.
- Input queues and result buffers are bounded.
- A slow task at the front causes head-of-line blocking; this is explicit semantics.
- Execution fails fast by default: the first error stops pulling new input and cancels unfinished tasks.
- Workers receive frozen snapshots of input values and captured closure values.
- Capturing unsendable values such as a `let mut` Cell, Stream, File handle, or Request body stream raises CheckError.
- The Runtime may use a thread pool internally, but does not expose threads, locks, or async/await to the language.
- Under `--dry-run`, `parallel` creates no worker processes. The Runtime executes closures as a lazy, ordered sequential
  map, so effects inside the closure remain visible in the plan while plan inspection itself has no
  process side effects.

To flatten streams returned by concurrent operations, use an explicit step:

```hhy-snippet
items
    |> parallel(4) { item -> process(item) }
    |> flat_map { stream -> stream }
```

## 24. Error Model

Error fields:

```text
kind code message source stage cause stack context
```

Error categories include at least:

```text
SyntaxError CheckError TypeError ValueError
IndexError KeyError EncodingError
IoError ProcessError HttpError HttpStatusError
TimeoutError CancelledError ResourceLimitError PlanError
```

### 24.1 Default Propagation

- Ordinary function errors propagate immediately to the caller.
- Failure of one Stream item terminates the entire Stream by default.
- Unhandled errors cause a nonzero script exit status.
- Error context excludes complete file contents, credentials, and sensitive HTTP headers by default.

### 24.2 Try/Catch

```hhy
try {
    let data = read_text(path("config.json")) |> parse_json
    print(data)
} catch err {
    err |> print_error
}
```

`catch` catches the first error propagated from the entire try block. Execution continues after catch completes normally; `throw(err)` can rethrow it.

### 24.3 Flow on_error

```hhy-snippet
pipeline
    |> on_error { err ->
        print_error(err)
        return fallback_stream
    }
```

`on_error` handles failure of the entire upstream. Its handler must return a replacement value of the same category as upstream, or rethrow; erroneous items are not implicitly skipped.

### 24.4 Per-Item Errors

`attempt` converts each execution outcome to Result:

```hhy-snippet
files
    |> map { file -> attempt { read_text(file.path) } }
    |> where { result -> result.ok }
    |> map { result -> result.value }
```

Result has fields `ok`, `value`, and `error`, making continued processing of failed items explicit.

`attempt { ... }` is a language-level expression: it executes the block and catches propagated errors. On success, Result.value is the value of the block's last expression; on failure, Result.error holds the error. An empty successful block has value null.

### 24.5 Resource Cleanup

- Every Stream operator's close must be idempotent.
- Error, return, exit, timeout, and Ctrl+C follow the same unwind/close path.
- Failed atomic file output removes temporary files and preserves the old file.
- Subprocesses, HTTP responses, watchers, and workers must all respond to cancellation.

## 25. Module System

Module syntax:

```hhy-snippet
import "./lib/report.hhy" as report
import { parse, validate } from "./lib/data.hhy"
import http

export let version = "1.0"

export fn build_report(data) {
    return report.build(data)
}
```

Rules:

- Relative imports resolve against the current source file's directory.
- Standard library modules use bare names; local modules use relative or absolute Paths.
- Standard library names take precedence over same-named files in the current directory; local files require explicit relative paths.
- Modules have independent top-level scopes.
- A module executes once on its first import and is cached.
- Cyclic imports raise CheckError before execution.
- Only names marked `export` are externally visible.
- Module top levels may read env but cannot read the caller's local variables.
- `args` is visible read-only in every module.
- v1.0 excludes remote imports and a public package repository.

## 26. Extension Architecture and Future Compatibility Boundaries

HHY Core owns Flow, the value model, errors, cancellation, and resource limits; extensions provide new data sources, types, and actions that can participate in Flow.

```text
HHY Core
  + Extension Types
  + Extension Sources
  + Extension Operators
  + Extension Actions
  = Same Flow Model
```

For example, a future Office extension does not need to change Pipe semantics:

```hhy-snippet
import office.excel

path("employees.xlsx")
    |> office.excel.open
    |> office.excel.sheet("Employees")
    |> office.excel.rows({ header: true })
    |> where { row -> row.Active == true }
    |> collect
```

The extension provides Workbook and Worksheet; HHY Core still provides `where`, `collect`, Error, timeout, and cancellation.

### 26.1 Three-Layer Extension Model

#### Pure HHY Package

These consist of `.hhy` modules that compose language features and authorized standard library capabilities. They are portable, do not access Runtime memory layouts, and suit text processing, data transformation, validation rules, and reusable Flows.

```text
text-utils/
├── hhy.toml
└── src/main.hhy
```

#### Process Extension

The extension runs as a separate process and communicates with the HHY Runtime through a versioned protocol. This is the preferred approach for future third-party extensions.

```text
HHY Runtime <-> Extension Protocol <-> office-hhy process
```

- An extension crash does not directly corrupt Runtime memory.
- Extensions may be implemented in C, C++, Rust, Go, or other languages.
- The Runtime can impose timeout, cancellation, process, and memory limits.
- The protocol evolves independently of HHY's internal C ABI.
- Suitable for Office, databases, cloud services, and large third-party integrations.

#### Native Module

A Native Module loads directly into the HHY process and is appropriate only for official, trusted, performance-sensitive low-level components.

- Best performance and call latency.
- A plugin crash crashes the Runtime.
- The plugin has the host process's permissions.
- The strictest memory ownership and ABI compatibility requirements.
- v1.0 does not expose a third-party Native Module ABI.

### 26.2 Extension Registration Model

Extensions may register only ordinary functions or Flow contracts, not an execution model different from HHY's.

Example logical signatures for an Office extension:

```text
office.excel.open
Path -> Workbook

office.excel.sheet
Workbook, String -> Worksheet

office.excel.rows
Worksheet, Map -> Stream<Map>

office.excel.set_sheet
Workbook, String, List<Map>, Map -> Workbook

office.excel.save
Workbook, Path -> Null
```

Every registration must declare:

```text
qualified_name
kind: function | source | operator | action
input_contract
output_contract
lazy: true | false
effect: none | filesystem | process | network | custom
cancel: supported | unsupported
threading: main | worker | isolated_process
protocol_version
```

Although HHY is dynamically typed, the Runtime still uses contracts for pre-call checks, execution-plan display, and error diagnostics.

### 26.3 Process Extension Protocol Principles

v1.0 reserves boundaries only and does not implement the protocol. A later protocol must satisfy:

- Independent protocol versioning rather than reusing the HHY Runtime version.
- Stable IDs for requests and responses.
- Deterministic structured encoding for ordinary values.
- Chunked transport for large Bytes and Streams.
- Support for backpressure, cancellation, timeout, and extension-originated errors.
- Conversion of errors into HHY Error while preserving the extension name and stage.
- Conversion of abnormal extension termination into ExtensionCrashedError.
- Runtime-managed handle lifetimes; all handles become invalid when the process exits.
- No transport of raw pointers or host memory addresses.

### 26.4 Extension Types and Resources

Extension types fall into two categories:

- **Serializable value**: Copyable and encodable, such as ordinary records and Office Cell values.
- **Opaque handle**: Exists only inside the extension, such as a Workbook handle.

Opaque handle:

- Contains extension_id, handle_id, and generation.
- Does not support equality, hashing, JSON encoding, or sharing across Parallel workers.
- Released after use by the Runtime or explicit close.
- Old handles become invalid after the extension restarts.
- Pure HHY Packages cannot forge handles.

When an extension returns a Stream, the Runtime wraps it with the unified open/next/close contract, preserving HHY Stream backpressure and cancellation downstream.

### 26.5 Capabilities and Permissions

Future extension manifests must declare capabilities:

```toml
[package]
name = "office"
version = "1.0.0"
author = "HHY Official"

[extension]
kind = "process"
command = "hhy-office"
protocol = "1"

[capabilities]
read = ["*.xlsx"]
write = ["*.xlsx"]
network = []
```

- Installing an extension does not permanently grant all permissions.
- `author` is a required displayed author attribution; local installation does not treat this string as a signature or identity credential.
- The Runtime limits file, process, and network scope according to script input and CLI authorization.
- dry-run must display actions declared by extensions.
- Undeclared or unauthorized effects raise PermissionError before execution.

A full capability sandbox is not a v1.0 feature, but core APIs must pass through unified effect dispatch points so permission controls can be added later.

### 26.6 Packages and Versions

Future package manifests use `hhy.toml` and version the following separately:

```text
package_version
required_hhy_version
extension_protocol_version
native_abi_version
```

- Pure HHY Packages follow HHY language and standard library compatibility policies.
- Process Extensions depend only on the Extension Protocol, not on HhyValue memory layouts.
- Native Modules must match the Native ABI major version exactly.
- Package lockfiles record exact versions and integrity hashes.
- Remote installation must verify hashes and signatures; v1.0 does not implement remote installation.

### 26.7 Actual v1.0 Commitments

v1.0 commits only to:

- Syntax supporting dotted module-qualified names such as `office.excel`.
- The standard library does not occupy arbitrary third-party top-level namespaces.
- Clear internal boundaries for HhyValue, Stream, Error, CancellationToken, and effect dispatch.
- Standard library operators use the same logical contract descriptions as future extensions.
- The Checker raises ModuleNotFoundError for uninstalled modules rather than a syntax error.

v1.0 does not commit to:

- A package manager or public package repository.
- Third-party extension installation or loading.
- A stable Process Extension Protocol format.
- A public Native C ABI.
- The Office extension itself.

Suggested roadmap:

```text
v1.0  Core contracts and namespace reservation
v1.1  Process Extension Protocol + local package install
v1.2  official HTML extension as protocol validation
later public Native ABI after Runtime stabilization
```

See [ROADMAP.md](ROADMAP.md) for detailed milestones, compatibility commitments, and acceptance conditions. This document remains responsible for language semantics and core contracts; the roadmap does not redefine semantics.

## 27. CLI

```text
hhy script.hhy [args...]
hhy run script.hhy [args...]
hhy repl
hhy fmt script.hhy
hhy fmt --check
hhy check script.hhy
hhy bytecode script.hhy
hhy run --dry-run script.hhy
hhy profile [--cpu|--heap] [--format text|json] [--output path] script.hhy [args...]
hhy --version
hhy --help
```

- `hhy script.hhy` is shorthand for `hhy run script.hhy`.
- All arguments after `--` are placed unchanged in `args`.
- `repl` supports expressions, blocks, and multiline Pipes.
- `fmt` defines the sole official format, must be idempotent, and must preserve comments.
- `check` checks syntax, scope, modules, unsendable captures, and known standard library calls; it does not promise complete static type checking.
- Non-interactive output is stable and supports `NO_COLOR`.
- `--dry-run` intercepts file writes, process launches, and HTTP send.
- `profile` executes the script and reports CPU samples, call counts, and managed Heap allocations for logical functions
  and builtins. Reports default to stderr; script stdout and exit codes retain normal execution semantics.
- `profile` collects both CPU and Heap data by default; specifying only `--cpu` or `--heap` collects only the selected
  category. `--format json` provides a machine-readable report, and `--output` writes it to a file.
- `bytecode` compiles, verifies, and disassembles the internal Bytecode IR without executing it. `bytecode --metrics`
  outputs schema-1 compile/verify/prepare measurements for cache admission. Opcodes, memory layouts, and disk formats
  are not public compatibility commitments. Since v1.3.5, `run` uses Bytecode by default; `--engine ast` and
  `HHY_ENGINE=ast` permanently retain the AST semantic oracle and emergency fallback. Real v1.3.10 measurements did not meet
  cache admission criteria, so externally precompiled Bytecode is not accepted and no `.hhyc` cache exists.

Exit codes:

```text
0 success
1 unhandled runtime error
2 syntax/check error
3 invalid CLI usage
4 IO/process/network error
5 timeout/cancel
```

## 28. Security and Resource Limits

Each execution creates RuntimeLimits:

```text
max_memory
max_open_files
max_processes
max_parallelism
max_http_body
max_regex_steps
max_recursion
max_runtime
```

Defaults: `max_memory=512mib`, `max_open_files=256`, `max_processes=16`,
`max_parallelism=16`, `max_http_body=16mib`, `max_regex_steps=1000000`,
`max_recursion=256`; `max_runtime=0` means ordinary CLI scripts have no overall time limit by default.
Embedders pass overrides through `HhyRuntimeLimits`; the CLI uses repeatable
`--limit NAME=VALUE` arguments, for example:

```sh
hhy run --limit max_runtime=30s --limit max_memory=256mib script.hhy
```

Size values require `b/kb/mb/gb/kib/mib/gib`; time values require
`ns/us/ms/s/min/h`; counts have no units. Invalid or zero limits are CLI usage errors.

Default behavior:

- run bypasses the Shell.
- TLS verification is enabled.
- Sensitive headers and environment values are redacted.
- Recursive file traversal does not follow symlinks.
- Regex execution is bounded.
- Temporary files are created securely.
- Ctrl+C first cancels gracefully, then forces cleanup after a grace period.
- dry-run has no external side effects.

`max_memory` is the per-execution language heap limit measured relative to the Runtime's startup baseline. Near the limit,
the Runtime first triggers a full GC. If allocation still cannot be satisfied after collection, it raises
`ResourceLimitError(HHY_MEMORY_LIMIT)` and follows normal Stream close and resource unwind;
it must not call `abort` or terminate the host process directly with an internal error code.

v1.0 does not implement a full capability sandbox, but API design must not bypass future permission checkpoints.

## 29. C Runtime and Memory Ownership

### 29.1 HhyValue

HhyValue uses a tagged union for scalars; String, List, Map, Function, Error, and system objects point to GC-managed objects.

```text
scalar: Null Bool Int Float Bytes Duration Percent
heap:   String Regex List Map Function Error Result Stream system objects
```

### 29.2 Ownership Rules

- The v1.0 internal language heap uses Boehm–Demers–Weiser conservative GC; the public Native ABI is not frozen.
- The Runtime C API still distinguishes managed values, borrowed views, and explicit system resources; these must not be mixed.
- Managed values may be retained by environments, collections, closures, and Streams; GC reclaims them when unreachable.
- String, List, and Map are logically immutable and safe to share.
- Collection updates return new objects; implementations may use copy-on-write internally.
- HHY v1.0 has no in-place collection updates, so user syntax cannot directly construct self-referential collections; GC safely handles internal reference cycles.
- Closures capture immutable values; mutable Cells cannot enter workers across threads.
- File descriptors, processes, HTTP handles, watchers, and workers do not depend on GC finalizers. They are released explicitly through execution unwind and operator close.

### 29.3 Parallel Isolation

- Freezable immutable values cross into isolated workers through versioned binary snapshots; GC heap pointers are not shared.
- Mutable Cells, Streams, and open resource handles are unsendable.
- Worker results are converted into shareable immutable values before entering the main execution flow.
- v1.0 workers use isolated processes. The main Runtime and each worker have their own GC heap; results enter the main heap after deserialization.

### 29.4 Runtime Architecture

```text
Source
  -> Lexer
  -> Parser
  -> AST
  -> Resolver / Checker
  -> Execution Plan
  -> Runtime / Flow
  -> Platform / IO / HTTP
```

Suggested directories:

```text
include/hhy/   Internal shared interfaces
src/lexer/     Lexical analysis
src/parser/    Parsing
src/ast/       AST and printer
src/check/     Name and basic semantic checks
src/runtime/   Value, scopes, functions, Error, cancellation
src/flow/      Stream and operators
src/stdlib/    Files, text, JSON, CSV, system, HTTP
src/platform/  POSIX platform adaptation
src/cli/       run, repl, fmt, check
tests/         Unit, integration, end-to-end, and fuzz tests
```

v1.0 uses an AST interpreter; a bytecode VM, JIT, and LLVM are outside v1.0.

## 30. Third-Party Dependency Policy

The C implementation should not write high-risk protocols and parsers from scratch. Selection must assess licensing, maintenance, portability, and static-linking support.

Fixed boundaries:

- Use established HTTP/TLS libraries; do not implement custom TLS.
- JSON uses a tested, bounded, strict parser and encoder within the core.
- Regex uses PCRE2 8-bit with execution resource limits.
- File watching uses native platform APIs behind a unified platform interface.
- Unicode must use at least a verified UTF-8 library or centralized implementation rather than separate handling in each module.

Dependency versions, licenses, and release linking details are recorded in `DEPENDENCIES.md`.

## 31. Cross-Platform Support

v1.0 officially supports:

- macOS arm64.
- Linux x86_64.
- Linux arm64.

Windows is a v1.1 candidate and is not a v1.0 release blocker.

The platform layer unifies:

- Path separators and normalization.
- File metadata.
- Watch events.
- Process enumeration and termination.
- Signals and cancellation.
- Line endings and terminal capabilities.
- Executable lookup.

Unavailable system fields return null rather than fabricated values; standard library documentation lists platform differences.

## 32. Development Stages and Freeze Points

### v0.1:Grammar Freeze

- Lexer, formal EBNF, Parser, AST, and AST printer.
- Literals, expressions, variables, blocks, functions, closures, and Pipe.
- Freeze core grammar after syntax snapshot tests pass.

### v0.2:Runtime Freeze

- HhyValue, tracing GC, immutable collections, scopes, functions, Error, and Result.
- Numeric, Unicode, equality, and ownership tests.
- Freeze the Value ABI and memory rules.

### v0.3:Flow Freeze

- Stream, open/next/close, cancellation, map, flat_map, where, take, reduce, and print.
- Finite-stream, large-stream, and early-termination tests.
- Freeze Pipe and operator contracts.

### v0.4:Core Preview

- Path, files, read_lines, text, Regex, JSON, CSV, Units, and modules.
- Support real log and data-processing scripts.
- Expand system capabilities in parallel only after Core Preview.

### v0.5:System

- processes, run, env, system, and resource limits.

### v0.6:Network

- HttpRequest, policy, send, HttpResponse, TLS, retry, and timeout.

### v0.7:Automation

- watch, every, parallel, debounce, and unified cancellation.

### v0.8:Tooling

- REPL, fmt, check, error diagnostics, and documentation tools.

### v0.9:Release Hardening

- Performance, cross-platform support, installers, security audit, fuzzing, and documentation CI.

### v1.0:Stable

- Freeze grammar, modules, the error model, core standard library, and CLI.
- Publish a compatibility policy: v1.x may add non-keyword APIs without changing the semantics of valid v1.0 programs.

## 33. Testing Requirements

- Lexer token and error-location tests.
- Parser AST snapshots and error recovery tests.
- Formatter idempotence and comment preservation tests.
- Value, GC stress, collection immutability, and resource ownership tests.
- Unicode, numeric overflow, unit, and Regex tests.
- Stream laziness, backpressure, early termination, and close tests.
- PlanError tests for infinite streams entering barriers.
- File, JSON, CSV, process, and HTTP integration tests.
- Fault injection for retry, timeout, cancel, and atomic save.
- Watch platform normalization tests.
- Parallel ordering, buffering, error, and cancellation tests.
- AddressSanitizer and UndefinedBehaviorSanitizer.
- Release and Debug/Sanitizer use isolated object directories; `make test-debug` must not link against Release
  objects.
- Lexer, Parser, JSON, CSV, and Regex boundary fuzzing.
- `make fuzz-smoke` provides fast deterministic regression; `make fuzz-ci` must run coverage-guided libFuzzer + ASan/UBSan. Neither may be presented as the other.
- CI runs Parser + Checker on documentation blocks marked as complete `hhy`. Complete scenarios relying on files, processes, HTTP, or watchers must be executed by corresponding local acceptance scripts, not merely syntax-checked. Blocks illustrating partial syntax with deliberately omitted context must be marked `hhy-snippet`.

## 34. Explicit v1.0 Non-Goals

This list records the scope of the initial v1.0 release only, not missing features in the current v1.7.0 implementation.
Later compatible releases added process extensions, a signed Registry, a Bytecode VM, Windows MSYS2
releases, and Web Runtime. See the compatible implementation addendum at the beginning for current Web capabilities and boundaries.

- Databases and ORM.
- Web Framework, HTTP Server, and WebSocket.
- GUI.
- AI, machine learning, and natural-language execution.
- async/await, Actors, and public thread APIs.
- Mutable shared-memory concurrency.
- Complex static types, generics, traits, and macros.
- JIT, LLVM, native compilation, and a bytecode VM.
- Remote imports and an official package repository.
- Third-party extension installation, Process Extension Protocol, and a public Native C ABI.
- The Office extension itself; it is a separate project after the extension system stabilizes.
- Official Windows support.

## 35. Core Acceptance Programs

Reproducible local versions of the following five program categories live in `tests/acceptance/`. CI executes them using large-log fixtures,
a local HTTP server, real process enumeration, and native file watchers; syntax checking alone is insufficient.

### 35.1 Files, Text, Units, and Concurrency

```hhy
path("./logs")
    |> files("**/*.log")
    |> where { file -> file.size > 1mb }
    |> parallel(4) { file ->
        read_lines(file.path)
            |> where { line -> contains(line, "ERROR") }
            |> collect
    }
    |> flat_map { lines -> lines |> stream }
    |> save_lines(path("errors.txt"))
```

### 35.2 HTTP, JSON, and Errors

```hhy
http.get("https://api.example.com/users")
    |> timeout(5s)
    |> retry({ count: 3, backoff: 200ms })
    |> send
    |> response_body
    |> parse_json
    |> get("users")
    |> where { user -> user.active }
    |> map { user -> { name: user.name, email: user.email } }
    |> encode_json({ pretty: true })
    |> save_text(path("active-users.json"))
    |> on_error { err ->
        print_error(err)
        throw(err)
    }
```

### 35.3 Processes and System Information

```hhy
processes
    |> where { process ->
        process.memory > 1gb or process.cpu > 80%
    }
    |> sort_by({ order: "desc" }) { process -> process.memory }
    |> take(10)
    |> print
```

### 35.4 Watch Automation

```hhy
watch(path("./src"), { recursive: true })
    |> where { event -> event.path.extension == ".c" }
    |> debounce(200ms)
    |> for_each { event ->
        run(["make"], { timeout: 2min })
    }
```

### 35.5 General Language Features

```hhy-snippet
import { normalize } from "./lib/data.hhy"

fn summarize(items) {
    let mut total = 0

    for item in items {
        if item.enabled {
            total = total + item.score
        }
    }

    return {
        count: items |> stream |> count,
        total: total
    }
}

let result = summarize([
    { enabled: true, score: 10 },
    { enabled: false, score: 20 },
    { enabled: true, score: 30 }
])

result |> normalize |> encode_json |> print
```

## 36. Release Conditions

HHY v1.0 must satisfy all of the following:

- Formal EBNF, precedence, and newline rules are frozen.
- Pipe injection, closures, Map/Block disambiguation, and Stream contracts are frozen.
- Value ownership, GC boundaries, unsendable values, and resource release rules are frozen.
- Qualified module names, internal operator contracts, and effect dispatch points can support future extensions, but v1.0 does not load third-party extensions.
- All five categories of core acceptance scripts execute successfully.
- Behavior of finite streams, large-file streams, and infinite event streams is defined.
- HTTP request plans, retry, and timeout control only send operations that have not yet executed.
- Whole-stream and per-item Error propagation rules are defined.
- Parallel ordering, backpressure, error, cancellation, and isolation rules pass tests.
- File, network, and process failures do not leak resources or damage existing output.
- fmt output is stable and check detects basic semantic errors.
- Signed or checksummed installation archives exist for macOS/Linux.
- Documentation examples are executed by CI.
- Unit, integration, end-to-end, memory, fuzz, and cross-platform tests pass.
- Known limitations are publicly documented.

Public limitations are maintained centrally in [`KNOWN_LIMITATIONS.md`](KNOWN_LIMITATIONS.md), which must be included in release packages. Scattered implementation comments cannot replace a user-visible limitations list.

Feature count is not the success criterion for v1.0. Success means that after installing HHY, users can reliably process files, processes, networks, and structured data with the same Flow model, and programs always express clearly:

```text
source |> transform |> filter |> action
```

## 37. v1.0 Implementation Conformance Ledger

This table preserves implementation evidence from the v1.0 freeze. Its versions, platforms, and counts are historical records, not a complete description of the current compatible implementation. It does not change the specification above; passing on one platform cannot replace cross-platform release conditions.

| Release gate | Recorded evidence | Status |
|---|---|---|
| Grammar, precedence, newlines, Pipe, and Map/Block | Exact Lexer token and Parser AST snapshots; multiple-error recovery and strict import grammar; 21 complete examples in `HHY_V1.md` plus README examples pass Parser + Checker; stray `}` fuzz timeout regression | Verified on macOS, Linux arm64, and native Linux x86_64 |
| Value, String, Map, logical system types, GC, and resource unwind | Embedded U+0000, UTF-8 entry points, Bytes/BytesBuffer distinction, Result/File/Directory/FileEvent/Process/CommandResult tags, cross-worker serialization, deep List/Map equality, exclusion of system objects from Map-only APIs, null key existence, GC stress, max_memory, atomic output, and repeated file/process/HTTP failure unwind regressions | Verified with ASan/UBSan on macOS, Linux arm64, and native Linux x86_64 |
| Lazy Flow and finite/large-file/infinite event streams | Laziness, lazy processes snapshots, early close, barrier PlanError, rejection of unhashable group keys and incomparable sort keys, 200000-item GC Flow, and five acceptance program categories | Verified on macOS, Linux arm64, and native Linux x86_64 |
| Files, processes, HTTP, JSON, CSV, and Regex | Local files, real processes, local HTTP server, binary responses, UTF-16 surrogates, multiline CSV, and PCRE2 limit tests | Verified on macOS, Linux arm64, and native Linux x86_64 |
| Watch, parallel, cancellation, and error propagation | Native watchers, rename normalization, FileEvent worker snapshots, ordered workers, early close, fail-fast, and Ctrl+C/timeout regressions | Verified on macOS, Linux arm64, and native Linux x86_64 |
| CLI, REPL, fmt, check, and modules | CLI exit codes, multiline REPL Pipes, fmt idempotence, Checker, module cache/export/missing-qualified-name tests | Verified on macOS, Linux arm64, and native Linux x86_64 |
| Contract Registry, Execution Plan, and EffectDispatcher | Checker/Runtime share a 94-callable Registry with concrete input/output/threading metadata; implementation/registration consistency, placeholder-metadata rejection, qualified arity, uniqueness, dry-run file/process/network plans and redaction; dry-run parallel does not fork and preserves lazy sequential values; Error stage is the callable name | Verified with ASan/UBSan on macOS, Linux arm64, and native Linux x86_64 |
| macOS arm64 Release and checksummed archive | Native macOS arm64 `1.0.0` completed Debug ASan/UBSan, Release, full tests, fuzz-smoke, documentation, and checksummed archive; artifacts include `BUILD_INFO.txt`, third-party notices, and SHA-256; auditable run: [`#11 / 89ca409`](https://github.com/hh696-wq/hhy-vm/actions/runs/32817348334) | Verified |
| Coverage-guided fuzzing | Latest recorded Linux arm64 Clang/libFuzzer + ASan/UBSan run: 16 seconds, 201496 executions; a previously discovered non-progressing Parser recovery timeout was fixed and its sample added to the corpus; macOS fuzz-smoke passed 1000 inputs; native Linux x86_64 CI runs libFuzzer + ASan/UBSan | Verified on Linux arm64 and native Linux x86_64 |
| Linux arm64 | `1.0.0` on native GitHub Actions Ubuntu 24.04 arm64 completed architecture assertions, strict GCC compilation, Debug ASan/UBSan, Release, full tests, Clang/libFuzzer, documentation, archive, and SHA-256; auditable run: [`#11 / 89ca409`](https://github.com/hh696-wq/hhy-vm/actions/runs/32817348334) | Verified in native CI |
| Linux x86_64 | `1.0.0` on native GitHub Actions Ubuntu 24.04 x86_64 completed architecture assertions, Debug ASan/UBSan, Release, 94-contract validation, full tests, libFuzzer, documentation execution, archive contents, and SHA-256; auditable run: [`#11 / 89ca409`](https://github.com/hh696-wq/hhy-vm/actions/runs/32817348334) | Verified in native CI |

`VERSION` may be frozen to `1.0.0` only when every release condition has auditable passing evidence. Cross-architecture QEMU cannot replace native sanitizer evidence; emulator startup failure cannot be recorded as implementation success or failure.

## Appendix A: Core EBNF Draft

This EBNF is the grammar baseline for the current Parser and Formatter. If implementation reveals a conflict, update this section first and add regression tests.

```ebnf
program         = { separator | declaration } EOF ;
declaration     = let_decl | fn_decl | import_decl | export_decl | statement ;

let_decl        = "let" [ "mut" ] IDENT "=" expression terminator ;
fn_decl         = "fn" IDENT "(" [ parameters ] ")" block ;
parameters      = IDENT { "," IDENT } ;
import_decl     = "import" import_spec terminator ;
export_decl     = "export" ( let_decl | fn_decl ) ;
import_spec     = STRING [ "as" IDENT ]
                | "{" import_item { "," import_item } "}" "from" STRING
                | module_name ;
import_item     = IDENT [ "as" IDENT ] ;
module_name     = IDENT { "." IDENT } ;

statement       = if_stmt | for_stmt | while_stmt | try_stmt
                | return_stmt | break_stmt | continue_stmt
                | expression terminator ;
if_stmt         = "if" expression block
                  { "else" "if" expression block }
                  [ "else" block ] ;
for_stmt        = "for" IDENT "in" expression block ;
while_stmt      = "while" expression block ;
try_stmt        = "try" block "catch" IDENT block ;
return_stmt     = "return" [ expression ] terminator ;
break_stmt      = "break" terminator ;
continue_stmt   = "continue" terminator ;

block           = "{" { separator | declaration } "}" ;
expression      = assignment ;
assignment      = pipe [ "=" assignment ] ;
pipe            = coalesce { "|>" pipe_stage } ;
pipe_stage      = callable [ call_args ] [ closure ] ;
coalesce        = logical_or { "??" logical_or } ;
logical_or      = logical_and { "or" logical_and } ;
logical_and     = equality { "and" equality } ;
equality        = comparison { ( "==" | "!=" ) comparison } ;
comparison      = term { ( "<" | "<=" | ">" | ">=" ) term } ;
term            = factor { ( "+" | "-" ) factor } ;
factor          = unary { ( "*" | "/" | "%" ) unary } ;
unary           = ( "not" | "+" | "-" ) unary | postfix ;
postfix         = primary { call_args | index | member } ;
callable        = IDENT { member } ;
call_args       = "(" [ arguments ] ")" ;
arguments       = expression { "," expression } ;
index           = "[" expression "]" ;
member          = "." IDENT ;

closure         = "{" [ IDENT "->" ] closure_body "}" ;
closure_body    = expression | { separator | declaration } ;
primary         = literal | IDENT | list | map | attempt_expr
                | "(" expression ")" ;
attempt_expr    = "attempt" block ;
literal         = NULL | BOOL | INT | FLOAT | STRING | REGEX
                | BYTES | DURATION | PERCENT ;
list            = "[" [ expression { "," expression } [ "," ] ] "]" ;
map             = "{" [ map_entry { "," map_entry } [ "," ] ] "}" ;
map_entry       = ( IDENT | STRING ) ":" expression ;

terminator      = NEWLINE | ";" | implicit_before_rbrace ;
separator       = NEWLINE | ";" ;
```

Multi-statement closure bodies and Map literals are resolved by the parent syntactic context and covered by Parser regression tests. The Parser must not alter the AST by guessing field names, newlines, or token contents. Later changes to this disambiguation rule are grammar-breaking changes.
