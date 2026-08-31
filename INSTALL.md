# Installing HHY

Current stable version: `1.1.8` (from the repository `VERSION` file).
HHY v1.1 supports macOS arm64 and Linux x86_64/arm64.

## Install a release binary

Download the archive for your platform from the
[GitHub Releases page](https://github.com/hh696-wq/hhy-vm/releases/latest), then:

```sh
tar -xzf hhy-1.1.8-PLATFORM-ARCH.tar.gz
cd hhy-1.1.8-PLATFORM-ARCH
./bin/hhy --version
./bin/hhy run examples/07-language-basics.hhy
```

Release archives include required non-system runtime libraries. Keep `bin/`
and `lib/` together; add the extracted `bin` directory to `PATH` if desired.
Verify the archive with its `.sha256` file before running it.

Official local extension packages are included under `extensions/`. For example:

```sh
./bin/hhy install ./extensions/database
./bin/hhy list
```

## Dependencies for building from source

macOS with Homebrew:

```sh
brew install pcre2 bdw-gc jansson openssl@3
```

Ubuntu/Debian:

```sh
sudo apt-get install build-essential libcurl4-openssl-dev libpcre2-dev libgc-dev \
  libjansson-dev libssl-dev
```

运行开发用覆盖引导 fuzz 还需要 LLVM clang：macOS 使用 `brew install llvm`，
Ubuntu/Debian 使用 `sudo apt-get install clang`。普通构建和安装不需要 LLVM。

## Build and install

```sh
make
make test
sudo make install PREFIX=/usr/local
hhy --version
```

Building the bundled `database` process extension additionally requires `libpq`
and the MySQL client development library:

```sh
# macOS
brew install libpq mysql
make -C extensions/database

# Ubuntu/Debian
sudo apt-get install libpq-dev default-libmysqlclient-dev
make -C extensions/database
```

Building the bundled `html` process extension additionally requires Lexbor:

```sh
# macOS
brew install lexbor
make -C extensions/html

# Ubuntu/Debian: install the distribution's Lexbor development package,
# or build Lexbor from its official source release before running:
make -C extensions/html
```

For an isolated installation, set `DESTDIR`; for a Homebrew prefix use
`make install PREFIX="$(brew --prefix)"`. Release archives include a SHA-256
file. Verify it with `shasum -a 256 -c FILE.sha256` on macOS or
`sha256sum -c FILE.sha256` on Linux before installation.
