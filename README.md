<p align="center">
  <img src="docs/assets/hhy-logo.png" width="200" alt="HHY Language logo">
</p>

<h1 align="center">HHY Language</h1>
<p align="center"><strong>Pipe Everything.</strong></p>
<p align="center">Connect files, processes, networks, and structured data through one dataflow model.</p>

<p align="center">
  <strong>English</strong> · <a href="README.zh-CN.md">中文</a>
</p>

<p align="center">
  <a href="https://hhylang.dev/en">Website</a> ·
  <a href="https://hhylang.dev/en/learn">Language manual</a> ·
  <a href="docs/README.md">Documentation</a> ·
  <a href="https://github.com/hh696-wq/hhy-vm/releases/latest">Downloads</a> ·
  <a href="docs/HHY_V1.en.md">Language specification</a>
</p>

<p align="center">
  <a href="VERSION"><img src="https://img.shields.io/badge/version-1.7.0-0969da" alt="Version 1.7.0"></a>
  <a href="https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml"><img src="https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml/badge.svg" alt="CI status"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-0b7285" alt="Apache License 2.0"></a>
</p>

HHY is a system scripting language implemented in C. Programs follow `source |> transform |> action` to process files, collect data, automate tasks, and serve web applications. The language defines explicit syntax, type rules, execution semantics, and exit codes.

| Feature | Purpose |
| --- | --- |
| Flow-first | Compose sources, transformations, filters, and actions with `\|>` |
| System capabilities | Work with files, processes, HTTP, JSON, CSV, and file watchers |
| Script semantics | Variables, functions, closures, branches, loops, modules, and structured errors |
| Native units | Express sizes, durations, and ratios with `10mib`, `500ms`, `2h`, and `80%` |
| Bounded execution | Lazy Streams, bounded concurrency, cancellation, resource limits, and redacted dry runs |

[Quick start](#quick-start) · [Language tour](#language-tour) · [Web and extensions](#web-and-extensions) · [Command-line tools](#command-line-tools) · [Current version](#current-version) · [Development and documentation](#development-and-documentation)

## Quick start

### 1. Install

For macOS arm64 and Linux arm64 / x86_64:

```sh
curl -fsSL https://hhylang.dev/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
hhy --version
```

The installer selects the platform package and verifies SHA-256. [Installer source](install.sh) · [Installation guide](INSTALL.md)

<details>
<summary>Other installation methods: Homebrew, manual download, and source builds</summary>

**Homebrew (macOS arm64)**

```sh
brew tap hh696-wq/hhy https://github.com/hh696-wq/hhy-vm.git
brew install hhy
```

**Manual download**

Download your platform archive from [GitHub Releases](https://github.com/hh696-wq/hhy-vm/releases/latest) and verify it against `.sha256` or `SHA256SUMS`. Keep the extracted `bin/` and `lib/` directories together, then run `./bin/hhy`.

Windows x86_64 uses the MSYS2 release package, which does not include the Database extension.

**Build from source (macOS example)**

```sh
brew install curl pcre2 bdw-gc jansson openssl@3 zlib
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm
make
./build/hhy --version
```

See [INSTALL.md](INSTALL.md) and [dependencies](docs/DEPENDENCIES.md) for Linux packages, installation paths, and additional dependencies needed to build official extensions.

</details>

### 2. Write a script

Create `hello.hhy`:

```hhy
let language = "HHY"

["Flow", "Pipe", "System"]
    |> map { word -> "{language}: {word}" }
    |> print
```

### 3. Check and run

```sh
hhy check hello.hhy
hhy run hello.hhy
```

After downloading or cloning the repository, you can also run `hhy run examples/07-language-basics.hhy`. Continue with the [online quick start](https://hhylang.dev/en/learn/quick-start).

## Language tour

### Files and text

Read logs and print the first 20 lines containing `ERROR`. Prepare a `./logs` directory with log files before running this example.

```hhy
path("./logs")
    |> files("**/*.log")
    |> flat_map { file -> read_lines(file.path) }
    |> where { line -> contains(line, "ERROR") }
    |> take(20)
    |> print
```

### HTTP and JSON

Set request timeouts and retries, then parse the response. Replace the placeholder `https://example.com/users` with an endpoint that returns JSON.

```hhy
http.get("https://example.com/users")
    |> timeout(5s)
    |> retry(3)
    |> send
    |> response_body
    |> parse_json
    |> print
```

### Bounded concurrency

Process two requests concurrently while preserving input order in the output.

```hhy
["https://example.com", "https://example.org"]
    |> parallel(2) { url ->
        http.get(url)
            |> timeout(5s)
            |> send
    }
    |> print
```

### Process queries

Find processes using more than 1 GB of memory and return the top 10 in descending order. The host must allow access to process information.

```hhy
processes
    |> where { process -> process.memory > 1gb }
    |> sort_by({ order: "desc" }) { process -> process.memory }
    |> take(10)
    |> print
```

See [examples](examples/README.md) for more scripts. CI validates the complete HHY code blocks in this README with the Parser and Checker. File and network examples also require suitable inputs and host capabilities at runtime.

## Web and extensions

| Capability | Current support | Details |
| --- | --- | --- |
| Web Runtime | Persistent applications, routers, middleware, static files, uploads, SSE, Range, multiple workers, and metrics | [Web API and deployment](docs/WEB_RUNTIME.md) |
| Database 1.0.0 | MySQL/PostgreSQL, TLS, bounded connection pools, transactions/savepoints, prepared statements, cursors, and HHY Streams | [Database API](extensions/database/README.md) |
| HTML | Error-tolerant HTML5 parsing, CSS selectors, and structured extraction | [HTML extension](extensions/html/README.md) |
| Extension tooling | Process isolation, declared permissions, signed registries, locking, offline installation, and rollback | [Extensions](extensions/README.md) · [Registry protocol](docs/EXTENSION_REGISTRY_V1.md) |

Run the repository's web example:

```sh
hhy serve examples/10-web-api.hhy -- 8080
hhy serve --dev examples/10-web-api.hhy -- 8080
```

A reverse proxy handles TLS and HTTP/2. Remote Database connections require explicit configuration and endpoint authorization. Validation on actual RDS instances and 24-hour soak tests remains pending. See [known limitations](docs/KNOWN_LIMITATIONS.md).

## Command-line tools

| Common command | Purpose |
| --- | --- |
| `hhy run <script.hhy> [args...]` | Run a script |
| `hhy check <file.hhy>...` | Check syntax and core semantics |
| `hhy fmt <file.hhy>...` | Format source files |
| `hhy repl` | Start an interactive session |
| `hhy run --dry-run <file.hhy>` | Produce a redacted execution plan |
| `hhy profile <script.hhy>` | Profile CPU, call counts, and the managed heap |
| `hhy serve <app.hhy> [args...]` | Start a persistent web application |
| `hhy --help` | Show all arguments and resource-limit options |

<details>
<summary>Extension management, machine-readable diagnostics, and compiler debugging</summary>

| Command | Purpose |
| --- | --- |
| `hhy install <local-path>` | Validate and install a local process extension |
| `hhy install --registry DIR --trust-root FILE <namespace/name>` | Verify signatures, resolve dependencies, and install |
| `hhy lock --registry DIR --trust-root FILE <namespace/name>` | Write the locked dependency graph |
| `hhy fetch --locked ...` | Cache locked dependencies |
| `hhy install --locked --offline ... <namespace/name>` | Recreate dependencies offline |
| `hhy install --upgrade ...` / `hhy rollback <package>` | Upgrade or roll back |
| `hhy doctor extensions` | Inspect installation, locks, and caches |
| `hhy list` / `hhy remove <package>` | List or remove extensions |
| `hhy check --format json <file.hhy>...` | Emit structured diagnostics |
| `hhy contracts --format json` | Emit the Callable Contract Registry |
| `hhy bytecode <file.hhy>` | Compile, verify, and disassemble without execution |
| `hhy bytecode --metrics <file.hhy>` | Report compilation, verification, and preparation metrics |
| `hhy ast <file.hhy>` / `hhy tokens <file.hhy>` | Inspect the AST or tokens |

Editors and automation tools can consume these JSON interfaces. Editor plugin source and complete application projects are maintained separately from the Core repository.

</details>

Profiler reports go to stderr by default, preserving script stdout. You can also save JSON:

```sh
hhy profile --heap --format json --output profile.json script.hhy
```

Short tasks may not produce enough CPU samples. Heap metrics cover HHY-managed memory only, excluding allocations in extension processes and native libraries. [Profiling and optimization switches](docs/CURRENT_VERSION.md)

## Current version

Core is **1.7.0**, the language specification remains frozen at **1.0.0**, and the independently versioned Database extension is **1.0.0**.

| Area | Status |
| --- | --- |
| Default execution | Compiler/Verifier-validated Bytecode, with a permanent `--engine ast` fallback |
| Optional optimizations | Structured HIR, six static passes, integer MIR, parameter feedback guards/deoptimization, and local List scalar replacement |
| Enabling optimizations | Explicitly set `HHY_COMPILER=ir`, `HHY_FEEDBACK_SPECIALIZATION=1`, and `HHY_SCALAR_REPLACEMENT=1`; see the usage guide for dependencies |
| Performance boundary | New optimizations remain off by default; correctness and release acceptance do not imply broad speedups on real workloads |
| Resource boundary | List scalar replacement preserves GC/quota allocation reservations; it does not imply elimination of physical heap allocation |
| Bytecode cache | Not admitted; no persistent `.hhyc`, external precompiled Bytecode loader, or public Bytecode ABI |

### Major milestones

| Version stage | Delivered capabilities |
| --- | --- |
| v1.1–v1.2 | Diagnostics, process extensions, trusted distribution, locking/offline installation/rollback, and HTML validation |
| v1.3 | Bytecode VM, enabled by default since 1.3.5; subsequent Stream Kernel, Profiler, and cache governance work |
| v1.4 | Web Runtime, delivered together in 1.4.3 |
| v1.5 | Database 1.0.0 and Runtime integration |
| v1.6–v1.7 | Runtime profiling/experiments and the optimizing compiler, delivered together in 1.7.0 |

The v1.6.x and some v1.7.x numbers identify engineering stages, not separately installable releases. See [GitHub Releases](https://github.com/hh696-wq/hhy-vm/releases) for changes and the [public roadmap](docs/ROADMAP.md) for planned work.

<details>
<summary>View the HHY ecosystem diagram</summary>

![HHY ecosystem: Runtime, VM, and Compiler evolution](docs/assets/hhy-ecosystem.png)

Diagram versions identify stages of capability development. The table above and release notes define actual releases and default enablement.

</details>

## Development and documentation

This repository contains the language implementation, SDK, official extensions, tests, benchmarks, and public documentation. Complete applications, editors, the website, and raw performance reports are maintained separately. Required workload regressions remain in `tests/workloads/`.

| Goal | Resources |
| --- | --- |
| Install, learn, and look up syntax | [Installation](INSTALL.md) · [Documentation](docs/README.md) · [English specification](docs/HHY_V1.en.md) · [Chinese specification](docs/HHY_V1.md) |
| Understand the VM and compiler | [Bytecode](docs/BYTECODE.md) · [Compiler IR](docs/architecture/COMPILER_IR.md) |
| Understand Runtime experiments | [Calls and unwinding](docs/architecture/VM_CALL_RUNTIME.md) · [Inline cache](docs/architecture/VM_INLINE_CACHE.md) · [GC and scheduling](docs/architecture/VM_GC_SCHEDULER.md) |
| Contribute and validate changes | [Contributing](CONTRIBUTING.md) · [Runtime governance](docs/RUNTIME_GOVERNANCE.md) · [Workload regressions](tests/workloads/README.md) |
| Review validation and boundaries | [GitHub Actions](https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml) · [Known limitations](docs/KNOWN_LIMITATIONS.md) · [Security reporting](SECURITY.md) |

## License

HHY Language is licensed under [Apache License 2.0](LICENSE). Distributions must comply with the license and [NOTICE](NOTICE). Third-party dependencies retain their own licenses; see [third-party notices](docs/THIRD_PARTY_NOTICES.md).

<p align="center"><strong>Built solo. Designed to flow.</strong></p>
