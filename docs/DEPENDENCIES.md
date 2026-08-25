# HHY v1.0 第三方依赖

HHY 核心运行时保持小依赖面，只在协议、安全或 Unicode 语义不适合自行实现时使用成熟库。

| 依赖 | 用途 | 许可证 | 构建要求 |
|---|---|---|---|
| libcurl | HTTP/HTTPS、TLS 验证、代理与重定向 | curl license | 开发包和链接库 |
| PCRE2 8-bit | Regex `i/m/s/u`、命名捕获、执行限制 | BSD 3-Clause | `pcre2.h` 与 `libpcre2-8` |
| BDWGC | 长生命周期 HHY value 的保守追踪垃圾回收 | MIT-style permissive | `gc/gc.h` 与 `libgc` |
| LLVM clang/libFuzzer（仅开发测试） | 覆盖引导 Lexer、Parser、JSON、CSV 与 Regex fuzz | Apache-2.0 with LLVM exceptions | 不进入 HHY 发行二进制 |

依赖由操作系统或包管理器提供，不把动态库复制进源码仓库。macOS 可执行
`brew install curl pcre2 bdw-gc`；Linux 使用发行版对应的 libcurl、PCRE2 与 BDWGC 开发包。
正式发布包必须记录实际链接版本、许可证文本、目标平台和 SHA-256 校验值。
普通构建不依赖 LLVM；运行 `make fuzz-ci` 时 macOS 使用 Homebrew LLVM，Linux 使用 clang/libFuzzer。
