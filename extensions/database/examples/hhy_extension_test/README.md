# MySQL hhy_extension_test 示例

所有读写示例只连接专用测试库 `hhy_extension_test`，不使用业务数据库。

## 配置

```sh
cd extensions/database/examples/hhy_extension_test
cp config.example.json config.local.json
chmod 600 config.local.json
```

编辑 `config.local.json`：

```json
{
  "url": "mysql://root:你的密码@127.0.0.1:3306/hhy_extension_test",
  "database": "hhy_extension_test",
  "max_rows": 1000
}
```

`config.local.json` 已加入本目录 `.gitignore`，不会进入 Git。

## 只读：统计并列出所有表

从仓库根目录运行：

```sh
./build/hhy run extensions/database/examples/hhy_extension_test/read.hhy \
  extensions/database/examples/hhy_extension_test/config.local.json
```

该脚本执行：

```sql
SELECT COUNT(*) AS table_count
FROM information_schema.TABLES
WHERE TABLE_SCHEMA = ? AND TABLE_TYPE = 'BASE TABLE';
```

随后列出表名、存储引擎和估算行数。两个查询都使用 prepared statement。

## 创建测试库

如果测试库还不存在，可先使用一个有建库权限的 MySQL 配置运行：

```sh
./build/hhy run extensions/database/examples/hhy_extension_test/create-test-database.hhy \
  <可连接 MySQL 的配置文件>
```

写入脚本会在运行时强制检查库名必须是 `hhy_extension_test`。

## 写入演示

写入脚本只操作专用表 `_hhy_database_example`，并在成功结束时删除该表。必须
显式提供 `--write`：

```sh
./build/hhy run extensions/database/examples/hhy_extension_test/write-demo.hhy \
  extensions/database/examples/hhy_extension_test/config.local.json \
  --write
```

演示顺序为 CREATE、INSERT、SELECT、UPDATE、SELECT、DELETE、DROP。若执行中途
失败，专用表可能保留，可以手动执行：

```sql
DROP TABLE IF EXISTS hhy_extension_test._hhy_database_example;
```

不要把业务表名直接拼接进 SQL。当前扩展支持参数化值，不支持把表名或列名作为
`?` 参数传递。

## 事务提交与回滚

事务示例只在 `hhy_extension_test` 中创建临时测试表，分别验证一次成功提交和
一次重复主键触发的整体回滚，最后删除测试表：

```sh
./build/hhy run extensions/database/examples/hhy_extension_test/transaction.hhy \
  extensions/database/examples/hhy_extension_test/config.local.json \
  --write
```
