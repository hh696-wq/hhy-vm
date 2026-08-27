# HHY 扩展

HHY v1.1 使用隔离的 Process Extension。每个扩展位于独立子目录，通过
`hhy.toml` 声明包名、可执行文件、协议版本和最小 capability；扩展不会接触
HHY Runtime 私有的 C Value 内存布局。

每个清单必须声明 `author`。官方扩展填写 `HHY Official`，第三方扩展填写开发者
或组织名称。v1.1 仅支持本地安装，因此作者字段属于可见署名，不是数字签名。

## 当前扩展

| 目录 | 命名空间 | 用途 | 状态 |
| --- | --- | --- | --- |
| [`database`](database/) | `database.*` | PostgreSQL/MySQL 参数化查询、写入与事务 | 可安装使用 |
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

v1.1 首期只支持本地路径安装，不支持远程仓库下载。`hhy.*` 和 `std.*`
是保留命名空间，第三方包不能注册这些名称。协议细节见
[`docs/EXTENSION_PROTOCOL_V1.md`](../docs/EXTENSION_PROTOCOL_V1.md)。
