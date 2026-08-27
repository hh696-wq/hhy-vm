# HHY v1.1 第三方依赖

> 当前稳定版本：`1.1.1`；依赖政策适用于 v1.x。

HHY 核心运行时保持小依赖面，只在协议、安全或 Unicode 语义不适合自行实现时使用成熟库。

| 依赖 | 用途 | 许可证 | 构建要求 | 官方上游/发行页 |
|---|---|---|---|---|
| libcurl | HTTP/HTTPS、TLS 验证、代理与重定向 | curl license | 开发包和链接库 | [curl releases](https://curl.se/download.html) |
| PCRE2 8-bit | Regex `i/m/s/u`、命名捕获、执行限制 | BSD 3-Clause | `pcre2.h` 与 `libpcre2-8` | [PCRE2](https://www.pcre.org/) |
| BDWGC | 长生命周期 HHY value 的保守追踪垃圾回收 | MIT-style permissive | `gc/gc.h` 与 `libgc` | [BDWGC releases](https://github.com/bdwgc/bdwgc/releases) |
| Jansson | Process Extension Protocol 的确定性 JSON 编解码 | MIT | `jansson.h` 与 `libjansson` | [Jansson](https://digip.org/jansson/) |
| OpenSSL libcrypto | 本地扩展包 SHA-256 完整性校验 | Apache 2.0 | EVP API 与 `libcrypto` | [OpenSSL](https://www.openssl.org/) |
| libpq（database 扩展） | PostgreSQL 参数化查询 | PostgreSQL License | `libpq-fe.h` 与 `libpq` | [PostgreSQL](https://www.postgresql.org/) |
| MySQL Client（database 扩展） | MySQL prepared statement | GPL-2.0 with FOSS exception | `mysql.h` 与 `libmysqlclient` | [MySQL](https://www.mysql.com/) |
| Lexbor（html 扩展） | HTML Living Standard 解析、DOM 与 CSS Selector | Apache-2.0 | `lexbor/html/html.h` 与 `liblexbor` | [Lexbor](https://lexbor.com/) |
| LLVM clang/libFuzzer（仅开发测试） | 覆盖引导 Lexer、Parser、JSON、CSV 与 Regex fuzz | Apache-2.0 with LLVM exceptions | 不进入 HHY 发行二进制 | [LLVM releases](https://releases.llvm.org/) |

依赖由操作系统或包管理器提供，不把动态库复制进源码仓库。macOS 可执行
`brew install curl pcre2 bdw-gc jansson openssl@3`；database 扩展另需 `libpq mysql`，
html 扩展另需 `lexbor`。
Linux 使用发行版对应的开发包。
正式发布包通过 `BUILD_INFO.txt` 记录实际链接版本、编译器与目标平台，通过
`THIRD_PARTY_NOTICES.md` 附带许可证文本，并为归档文件生成 SHA-256 校验值。
普通构建不依赖 LLVM；运行 `make fuzz-ci` 时 macOS 使用 Homebrew LLVM，Linux 使用 clang/libFuzzer。
