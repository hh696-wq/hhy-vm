# 1. 快速开始

5 分钟安装 HHY、运行第一个 Flow，并掌握日常开发命令。

## 1.1 第 1 分钟：一键安装（推荐）

支持 macOS arm64、Linux x86_64 与 Linux arm64。安装器自动识别平台、下载 V1.2.0 发行包和同名 .sha256，校验通过后才安装；默认不需要 sudo。


```sh
curl -fsSL https://hhylang.dev/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
hhy --version
```


{% hint style="info" %}
默认安装版本目录为 ~/.local/share/hhy/1.2.0，命令入口为 ~/.local/bin/hhy。安装器默认解析 GitHub 最新稳定版；HHY_VERSION 可用于固定或回滚版本，HHY_INSTALL_ROOT 和 HHY_BIN_DIR 可以覆盖位置。
{% endhint %}


## 1.2 macOS：通过 Homebrew Tap 安装

Apple Silicon Mac 可以使用仓库内的 Formula。显式绑定 Git 仓库 URL，使当前仓库在独立 homebrew-tap 仓库建立前也能作为 Tap 使用。


```sh
brew tap hh696-wq/hhy https://github.com/hh696-wq/hhy-vm.git
brew install hhy
hhy --version
```


{% hint style="info" %}
当前 Formula 只支持 macOS arm64，并锁定官方发行包及其 SHA-256。Linux 请使用一键安装器或 Release 包。
{% endhint %}


## 1.3 方式二：直接下载 Release

不需要修改 HHY Runtime 时，直接使用官方 V1.2.0 发行包最快。根据系统和 CPU 选择 darwin-arm64、linux-x86_64 或 linux-arm64；压缩包已包含 HHY 可执行文件、官方示例与数据库扩展、所需的非系统运行库、文档、许可证和构建信息。


[打开 HHY GitHub Releases ↗](https://github.com/hh696-wq/hhy-vm/releases)

下载最新稳定版本、对应的 .sha256 文件或汇总 SHA256SUMS。


```sh
tar -xzf hhy-1.2.0-PLATFORM-ARCH.tar.gz
cd hhy-1.2.0-PLATFORM-ARCH
./bin/hhy --version
./bin/hhy run examples/07-language-basics.hhy
```


{% hint style="info" %}
保持 bin/ 与 lib/ 的相对位置不变，否则便携包可能找不到随包运行库。PLATFORM-ARCH 替换为 darwin-arm64、linux-x86_64 或 linux-arm64。
{% endhint %}


## 1.4 下载后校验与加入 PATH

运行下载内容前，应使用同名 .sha256 或 SHA256SUMS 验证文件完整性。macOS 自带 shasum，Linux 通常使用 sha256sum。


```sh
# macOS
shasum -a 256 -c hhy-1.2.0-darwin-arm64.tar.gz.sha256

# Linux
sha256sum -c hhy-1.2.0-linux-x86_64.tar.gz.sha256

# 当前终端加入 PATH（替换成实际绝对路径）
export PATH="/absolute/path/hhy-1.2.0-PLATFORM-ARCH/bin:$PATH"
hhy --version
```


长期使用时，把 export PATH 行放进 shell 配置文件；或者继续通过发行目录中的 ./bin/hhy 运行，不需要系统级安装。


## 1.5 方式三：从源码构建

需要开发 Runtime、验证最新源码或自定义安装位置时再选择源码构建。HHY V1.2.0 正式支持 macOS arm64、Linux arm64 和 Linux x86_64；需要 C11 编译器、make、libcurl、PCRE2 与 BDWGC。数据库扩展还需要对应的 PostgreSQL libpq 或 MySQL client 开发库。


```sh
brew install curl pcre2 bdw-gc
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm
make
make test
./build/hhy --version
```


{% hint style="info" %}
brew 命令只适用于 macOS。Linux 的依赖包名称因发行版而异，完整说明见仓库 INSTALL.md。
{% endhint %}


## 1.6 安装源码构建结果

```sh
make install PREFIX="$(brew --prefix)"
hhy --version
```


PREFIX 可以换成自定义绝对路径。确认 PREFIX/bin 已在 PATH 后，所有 .hhy 文件都可以通过 hhy run 执行。


## 1.7 第一个脚本

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


let 创建绑定；List 字面量保存三个 String；|> 把左侧值注入下一个函数；map 的闭包逐项生成新 String；print 消费结果。check 先验证词法、语法、作用域、模块与已知标准库调用，不执行副作用。


## 1.8 脚本运行与开发流程

| 任务 | 命令 | 用途 |
| --- | --- | --- |
| 格式化 | hhy fmt script.hhy | 写入 HHY 官方格式 |
| 检查格式 | hhy fmt --check script.hhy | 在 CI 中检查，不修改文件 |
| 检查脚本 | hhy check script.hhy | 检查语法、作用域和已知 API |
| 运行脚本 | hhy run script.hhy | 执行脚本 |
| 传递参数 | hhy run script.hhy input.csv output.json | 参数进入只读 args |
| 预览计划 | hhy run --dry-run script.hhy | 查看脱敏计划，不执行外部副作用 |


源码使用 .hhy 后缀。查看完整命令：


```sh
hhy --help
```
