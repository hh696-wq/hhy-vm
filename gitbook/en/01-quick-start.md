# 1. Quick Start

Install HHY, run your first Flow, and learn the daily commands in five minutes.

## 1.1 Minute 1: one-command install (recommended)

The installer supports macOS arm64, Linux x86_64, and Linux arm64. It detects the platform, downloads the V1.2.0 archive and matching .sha256, and installs only after verification. sudo is not required by default.


```sh
curl -fsSL https://hhylang.dev/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
hhy --version
```


{% hint style="info" %}
The default version directory is ~/.local/share/hhy/1.2.0 and the command entry point is ~/.local/bin/hhy. The installer resolves the latest stable GitHub release by default; use HHY_VERSION to pin or roll back, and HHY_INSTALL_ROOT or HHY_BIN_DIR to override locations.
{% endhint %}


## 1.2 macOS: install from the Homebrew tap

Apple Silicon Macs can use the Formula maintained in this repository. The explicit Git URL lets the repository act as a tap before a separate homebrew-tap repository is created.


```sh
brew tap hh696-wq/hhy https://github.com/hh696-wq/hhy-vm.git
brew install hhy
hhy --version
```


{% hint style="info" %}
The current Formula supports macOS arm64 only and pins the official archive and SHA-256. Use the installer or a Release archive on Linux.
{% endhint %}


## 1.3 Option 2: download a Release

If you are not modifying the HHY Runtime, an official V1.2.0 archive is the fastest path. Choose darwin-arm64, linux-x86_64, or linux-arm64 for your OS and CPU. Archives include the executable, official sample and database extensions, required non-system runtime libraries, docs, licenses, and build metadata.


[Open HHY GitHub Releases ↗](https://github.com/hh696-wq/hhy-vm/releases)

Download the latest stable archive and its matching .sha256 file or SHA256SUMS.


```sh
tar -xzf hhy-1.2.0-PLATFORM-ARCH.tar.gz
cd hhy-1.2.0-PLATFORM-ARCH
./bin/hhy --version
./bin/hhy run examples/07-language-basics.hhy
```


{% hint style="info" %}
Keep bin/ and lib/ in their original relative positions so the portable executable can find bundled libraries. Replace PLATFORM-ARCH with darwin-arm64, linux-x86_64, or linux-arm64.
{% endhint %}


## 1.4 Verify the download and add it to PATH

Before running a download, verify it with the matching .sha256 file or SHA256SUMS. macOS includes shasum; Linux commonly provides sha256sum.


```sh
# macOS
shasum -a 256 -c hhy-1.2.0-darwin-arm64.tar.gz.sha256

# Linux
sha256sum -c hhy-1.2.0-linux-x86_64.tar.gz.sha256

# Add to PATH for this terminal (use the real absolute path)
export PATH="/absolute/path/hhy-1.2.0-PLATFORM-ARCH/bin:$PATH"
hhy --version
```


For permanent access, put the export PATH line in your shell profile. You may also keep invoking ./bin/hhy from the extracted directory without a system-wide install.


## 1.5 Option 3: build from source

Build from source when developing the Runtime, validating current source, or choosing a custom installation prefix. HHY V1.2.0 supports macOS arm64, Linux arm64, and Linux x86_64 and requires a C11 compiler, make, libcurl, PCRE2, and BDWGC. The database extension additionally needs the corresponding PostgreSQL libpq or MySQL client development library.


```sh
brew install curl pcre2 bdw-gc
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm
make
make test
./build/hhy --version
```


{% hint style="info" %}
The brew command applies only to macOS. Linux package names vary by distribution; see INSTALL.md for the full dependency matrix.
{% endhint %}


## 1.6 Install the source build

```sh
make install PREFIX="$(brew --prefix)"
hhy --version
```


PREFIX may be a custom absolute path. Once PREFIX/bin is on PATH, execute any .hhy file with hhy run.


## 1.7 Your first script

**hello.hhy**

```hhy
let language = "HHY"

["Flow", "Pipe", "System"]
    |> map { word -> "{language}: {word}" }
    |> print
```


```sh
hhy check hello.hhy
hhy run hello.hhy
```


let creates a binding; the List literal stores three Strings; |> injects the left value into the next call; the map closure creates one new String per item; print consumes the result. check validates lexical syntax, scope, modules, and known standard-library calls without effects.


## 1.8 Running scripts and the development workflow

| Task | Command | Purpose |
| --- | --- | --- |
| Format | hhy fmt script.hhy | Write canonical HHY formatting |
| Check format | hhy fmt --check script.hhy | Verify in CI without changing files |
| Check script | hhy check script.hhy | Validate syntax, scope, and known APIs |
| Run | hhy run script.hhy | Execute the script |
| Pass arguments | hhy run script.hhy input.csv output.json | Arguments enter read-only args |
| Preview plan | hhy run --dry-run script.hhy | Inspect a redacted plan without external effects |


HHY source files use the .hhy suffix. View complete command help:


```sh
hhy --help
```
