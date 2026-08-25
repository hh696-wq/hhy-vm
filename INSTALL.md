# Installing HHY

Current stable version: `1.0.0` (from the repository `VERSION` file).
HHY v1.0 supports macOS arm64 and Linux x86_64/arm64.

## Dependencies

macOS with Homebrew:

```sh
brew install pcre2 bdw-gc
```

Ubuntu/Debian:

```sh
sudo apt-get install build-essential libcurl4-openssl-dev libpcre2-dev libgc-dev
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

For an isolated installation, set `DESTDIR`; for a Homebrew prefix use
`make install PREFIX="$(brew --prefix)"`. Release archives include a SHA-256
file. Verify it with `shasum -a 256 -c FILE.sha256` on macOS or
`sha256sum -c FILE.sha256` on Linux before installation.
