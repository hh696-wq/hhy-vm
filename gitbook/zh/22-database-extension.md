# 22. 数据库扩展使用指南

Database 1.0.0：MySQL/PostgreSQL 连接池、远程 TLS、事务、预处理、游标、精确类型与 HHY Stream，最低 HHY 1.5.0，已验证兼容 1.7.0。

## 22.1 版本与安装

{% hint style="info" %}
DB 1.0.0 于 2026-09-07 随 HHY 1.5.0 发布。数据库扩展独立版本化；支持 macOS arm64、Linux arm64/x86_64，Windows Runtime 包不包含 DB。
{% endhint %}


[下载 Database 1.0.0](https://github.com/hh696-wq/hhy-vm/releases/tag/database-v1.0.0)

HHY 1.5.0 · Database 1.0.0


```sh
hhy --version
# Extract the matching release archive and preserve bin/ and lib/
hhy install ./hhy-database-1.0.0-darwin-arm64
hhy list

# Or build from the repository root
make -C extensions/database
./build/hhy install ./extensions/database
```


## 22.2 连接配置与远程 TLS

**config.local.json**

```text
{
  "driver": "mysql",
  "host": "127.0.0.1",
  "port": 3306,
  "user": "cms_test",
  "password": "CHANGE_ME",
  "database": "hhy_test",
  "allow": [
    "127.0.0.1:3306"
  ],
  "tls": "disabled",
  "max_open": 4,
  "max_idle": 2,
  "connect_ms": 5000,
  "acquire_ms": 1000,
  "timeout_ms": 30000
}
```


{% hint style="info" %}
示例只连接专用本地测试库。保护配置文件并排除 Git；生产凭据从环境或密钥来源读取。远程数据库必须使用配置 Map：host 填真实地址，allow 精确列出 host:port，tls 使用 verify_identity，ca 指向 CA 证书。VPC、白名单与数据库授权由部署环境负责；RDS 实机尚未验收。
{% endhint %}


旧 URL 调用继续支持 loopback 地址；远程连接和池选项使用 Map。MySQL 占位符为 ?，PostgreSQL 为 $1、$2；标识符不能作为值参数。allow 是可信脚本策略，不是操作系统沙箱。


## 22.3 连接池与查询

**query.hhy**

```hhy
import database
let cfg = read_text(path(args[0])) |> parse_json
database.query(cfg, "SELECT ? AS value", [42], { typed: true }) |> print
database.stats() |> print
```


```sh
hhy run query.hhy ./config.local.json
```


连接池按数据源和扩展进程隔离。max_open 默认 4、max_idle 默认 2；多 Worker 的总连接数需要累加。支持空闲/寿命回收、连接重置、借用超时和统计。Duration 配置如 timeout_ms: 5s 可直接传给 DB。默认结果保持 String/null；typed 模式保留数值类型，Decimal/大整数用精确封装，二进制映射 BytesBuffer。


## 22.4 读写事务与流式查询

{% hint style="info" %}
以下示例要求已存在 pages(id, title) 测试表；PostgreSQL 请替换占位符。事务闭包成功提交、异常回滚。MySQL DDL 不属于多步原子事务；安装程序需记录迁移检查点。
{% endhint %}


**transaction.hhy**

```hhy
import database
let cfg = read_text(path(args[0])) |> parse_json
cfg |> database.with_transaction { tx ->
    database.execute(tx, "UPDATE pages SET title = ? WHERE id = ?", ["Hello HHY", 1])
    database.query(tx, "SELECT id, title FROM pages WHERE id = ?", [1])
} |> print
```


**stream.hhy**

```hhy
import database
let cfg = read_text(path(args[0])) |> parse_json
database.stream(cfg, "SELECT id, title FROM pages ORDER BY id", [])
    |> take(100)
    |> collect
    |> print
```


Stream 按批拉取，take 会提前关闭结果。句柄不能跨请求、Worker 或 close 后使用；请求结束清理遗留资源。未读完的结果可能导致物理连接丢弃，在事务内提前结束流时需留意事务失效。


## 22.5 API 与边界

| API | 用途 |
| --- | --- |
| ping / query / execute | 连接检查、查询与写入 |
| connection / begin / commit / rollback / close | 作用域连接和事务 |
| with_transaction / savepoint / rollback_to / release_savepoint | 事务闭包与保存点 |
| prepare / query_prepared / execute_prepared | 同一连接复用服务端预处理 |
| transaction / execute_many | 1–100 条原子批量 DML |
| cursor / fetch / next_result / close_result / stream | 增量结果；next_result 支持 MySQL |
| stats / capabilities | 指标与版本能力 |


{% hint style="info" %}
字段上限 64 KiB、协议消息 1 MiB，有界 query 最多 10000 行并报告截断。池饱和返回 DB_POOL_TIMEOUT；取消和超时可能销毁连接。DB_COMMIT_UNKNOWN 表示提交结果不确定，不能自动重放写入。
{% endhint %}


## 22.6 验收与安装场景

MySQL 8.4/PostgreSQL 17、本地与 CI、AST/Bytecode、双 Worker、TLS 正反例、取消、sanitizer、百万行本地流式已验收。install.hhy 数据库夹具覆盖失败恢复、重复安装、升级回滚和备份恢复；不是完整 CMS。每库本地持续测试 5 分钟，未宣称 24 小时长稳或 RDS 故障切换通过。


[完整 API 与迁移说明](/docs/DATABASE_1.0.0_README.md)

HHY 1.5.0 · Database 1.0.0


[原始数据摘要与重现方式](/docs/DATABASE_1.0.0_ACCEPTANCE.md)

HHY 1.5.0 · Database 1.0.0


[查看通过的发行 Actions](https://github.com/hh696-wq/hhy-vm/actions/runs/34079510695)

HHY 1.5.0 · Database 1.0.0
