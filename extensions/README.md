# HHY 扩展

HHY v1.5.0 使用隔离的 Process Extension。每个扩展位于独立子目录，通过
`hhy.toml` 声明包名、可执行文件、协议版本和最小 capability；扩展不会接触
HHY Runtime 私有的 C Value 内存布局。

每个清单必须声明 `author`。官方扩展填写 `HHY Official`，第三方扩展填写开发者
或组织名称。author 是可见署名；Registry 的可信来源由签名验证，不由作者文字保证。

## 当前扩展

| 目录 | 命名空间 | 用途 | 状态 |
| --- | --- | --- | --- |
| [`database`](database/) | `database.*` | DB 1.0.0：MySQL/PostgreSQL 池、远程 TLS、事务、游标与精确类型 | 要求 HHY 1.5.0；macOS/Linux |
| [`html`](html/) | `html.*` | HTML5 容错解析、CSS Selector 与结构化字段抽取 | 可安装使用 |
| [`sample`](sample/) | `sample.*` | Protocol v1 握手、注册、调用和错误传播测试 | 参考扩展 |

## 本地安装

```sh
make
make -C extensions/database

./build/hhy install ./extensions/database
./build/hhy list
```

脚本通过包名导入扩展：

```hhy-snippet
import database

let url = require_env("DATABASE_URL")
database.query(url, "SELECT 1 AS value", [], 10)
    |> get("rows")
    |> print
```

移除扩展：

```sh
./build/hhy remove database
```

当前支持本地路径安装与签名 Registry 分发。DB 1.0.0 另提供独立 [GitHub Release](https://github.com/hh696-wq/hhy-vm/releases/tag/database-v1.0.0)，Registry 版本以签名索引为准。`hhy.*` 和 `std.*`
是保留命名空间，第三方包不能注册这些名称。协议细节见
[`docs/EXTENSION_PROTOCOL_V1.md`](../docs/EXTENSION_PROTOCOL_V1.md)。

DB 的作用域句柄、取消、池预算与类型边界见 [Database API](database/README.md)。通用扩展不自动获得数据库资源语义。
