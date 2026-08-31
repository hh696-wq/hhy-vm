# 21. 数据库扩展使用指南

安装官方 database 0.2.0 扩展，通过 JSON 配置连接 MySQL/PostgreSQL，并完成查询、写入与事务。

## 21.1 database 扩展是什么

{% hint style="info" %}
database 0.2.0 随 HHY v1.1.0 发布，是仓库中真实可运行的 C11 进程扩展。当前支持 MySQL、PostgreSQL、参数化查询、参数化写入和第一版事务；连接 handle、连接池、流式查询与完整数据库类型映射属于后续版本。
{% endhint %}


| Callable | 用途 | 当前边界 |
| --- | --- | --- |
| database.ping(url) | 验证连接并返回数据库信息 | 每次调用建立短连接 |
| database.query(url, sql, params, max_rows?) | 执行有界参数化查询 | 结果包含 columns 与 rows |
| database.execute(url, sql, params) | 执行参数化写入或受控 DDL | 返回受影响行数 |
| database.transaction(url, statements) | 原子执行 1–100 条写语句 | 仅 INSERT/UPDATE/DELETE；失败整体回滚 |


## 21.2 安装扩展与四个 callable

```sh
make -C extensions/database
./build/hhy install ./extensions/database
./build/hhy list
```


install 会校验 hhy.toml、HHY 版本范围、扩展命令和 SHA-256 完整性，并在安装前展示网络 capability。安装成功后，脚本中的 import database 会启动隔离扩展进程、完成 Protocol 1 握手并注册四个 callable。


## 21.3 连接配置与凭据安全

```sh
cd extensions/database/examples/hhy_extension_test
cp config.example.json config.local.json
chmod 600 config.local.json
```


**config.local.json**

```text
{
  "url": "mysql://root:CHANGE_ME@127.0.0.1:3306/hhy_extension_test",
  "database": "hhy_extension_test",
  "max_rows": 1000
}
```


| 驱动 | 连接 URL 示例 | 参数占位符 |
| --- | --- | --- |
| MySQL | mysql://user:password@127.0.0.1:3306/hhy_extension_test | ? |
| PostgreSQL | postgresql://user:password@127.0.0.1:5432/hhy_extension_test | $1、$2…… |


{% hint style="info" %}
把 CHANGE_ME 替换为本机密码。config.local.json 已加入示例目录的 .gitignore；不要把真实密码写进 .hhy 源码、文档或 Git。示例脚本还会强制检查 database 必须是 hhy_extension_test，避免误操作业务库。
{% endhint %}


## 21.4 实战一：只读检查测试库

```sh
./build/hhy run \
  extensions/database/examples/hhy_extension_test/read.hhy \
  extensions/database/examples/hhy_extension_test/config.local.json
```


**read.hhy**

```hhy
import database
import { load_database_config } from "./lib/config.hhy"

let config = load_database_config(args[0])
let result = database.query(
    config.url,
    "SELECT COUNT(*) AS table_count FROM information_schema.TABLES WHERE TABLE_SCHEMA = ? AND TABLE_TYPE = 'BASE TABLE'",
    [config.database],
    1
)

print("Database", config.database)
print("Table count", result.rows[0].table_count)
```


仓库中的完整 read.hhy 还会列出每张表的名称、存储引擎和估算行数。SQL 值通过 params 传给驱动的 prepared statement，不会拼接进查询文本。


## 21.5 实战二：受控写入与事务

```sh
./build/hhy run extensions/database/examples/hhy_extension_test/write-demo.hhy \
  extensions/database/examples/hhy_extension_test/config.local.json --write

./build/hhy run extensions/database/examples/hhy_extension_test/transaction.hhy \
  extensions/database/examples/hhy_extension_test/config.local.json --write
```


**transaction-example.hhy**

```hhy
database.transaction(config.url, [
    { sql: "INSERT INTO _hhy_transaction_test (id, message) VALUES (?, ?)", params: [1, "created"] },
    { sql: "UPDATE _hhy_transaction_test SET message = ? WHERE id = ?", params: ["committed", 1] }
]) |> print
```


{% hint style="info" %}
两个写示例都要求显式 --write，并且只操作 hhy_extension_test 中的专用临时表。transaction 不接受 SELECT 或 DDL；任一语句失败时扩展会回滚整个事务。
{% endhint %}


## 21.6 当前边界与错误排查

| 现象 | 检查项 |
| --- | --- |
| ModuleNotFoundError | 先执行 install，并用 hhy list 确认 database 0.2.0 已安装 |
| cannot open .../read.hhy | 从仓库根目录运行完整路径，目录名是 hhy_extension_test |
| 连接失败 | 检查服务、端口、用户名、密码、库名及 hhy.toml 声明的本机网络范围 |
| SQL 参数错误 | MySQL 使用 ?；PostgreSQL 使用 $1、$2……；标识符不能作为值参数 |


[继续阅读扩展系统原理](/zh/learn/extensions-roadmap)

了解清单校验、权限声明、进程加载、Protocol 1 握手和扩展开发者约束。
