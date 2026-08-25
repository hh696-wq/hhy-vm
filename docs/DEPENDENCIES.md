# HHY v1.0 第三方依赖

> 当前稳定版本：`1.0.0`；依赖政策适用于 v1.0。

HHY 核心运行时保持小依赖面，只在协议、安全或 Unicode 语义不适合自行实现时使用成熟库。

| 依赖 | 用途 | 许可证 | 构建要求 | 官方上游/发行页 |
|---|---|---|---|---|
| libcurl | HTTP/HTTPS、TLS 验证、代理与重定向 | curl license | 开发包和链接库 | [curl releases](https://curl.se/download.html) |
| PCRE2 8-bit | Regex `i/m/s/u`、命名捕获、执行限制 | BSD 3-Clause | `pcre2.h` 与 `libpcre2-8` | [PCRE2](https://www.pcre.org/) |
| BDWGC | 长生命周期 HHY value 的保守追踪垃圾回收 | MIT-style permissive | `gc/gc.h` 与 `libgc` | [BDWGC releases](https://github.com/bdwgc/bdwgc/releases) |
| LLVM clang/libFuzzer（仅开发测试） | 覆盖引导 Lexer、Parser、JSON、CSV 与 Regex fuzz | Apache-2.0 with LLVM exceptions | 不进入 HHY 发行二进制 | [LLVM releases](https://releases.llvm.org/) |

依赖由操作系统或包管理器提供，不把动态库复制进源码仓库。macOS 可执行
`brew install curl pcre2 bdw-gc`；Linux 使用发行版对应的 libcurl、PCRE2 与 BDWGC 开发包。
正式发布包通过 `BUILD_INFO.txt` 记录实际链接版本、编译器与目标平台，通过
`THIRD_PARTY_NOTICES.md` 附带许可证文本，并为归档文件生成 SHA-256 校验值。
普通构建不依赖 LLVM；运行 `make fuzz-ci` 时 macOS 使用 Homebrew LLVM，Linux 使用 clang/libFuzzer。
