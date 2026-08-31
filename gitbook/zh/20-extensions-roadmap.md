# 20. 扩展系统

v1.2.0 进程扩展机制：官方签名 Registry、源码编译、包清单、权限声明、Protocol 1 与 callable 注册。

## 20.1 当前已实现的扩展边界

{% hint style="info" %}
v1.2.0 已实现本地 install/list/remove、Ed25519 签名 Registry、manifest 与 SHA-256 校验、隔离进程握手、动态 callable 注册、同步调用、结构化错误和 shutdown。脚本可以直接 import 已安装的扩展包。
{% endhint %}


| 能力 | 当前状态 | 边界 |
| --- | --- | --- |
| 扩展分发 | 已实现 | 官方签名 Registry 或本地源码构建；安装前验证身份、target、签名与文件哈希 |
| 进程协议 | 已实现 | handshake、register、call、call_result、error、shutdown |
| 值传输 | 已实现 | Null、Bool、数字、String、List、Map 的 JSON 协议映射 |
| Stream / handle / cancel | 未实现 | 属于后续协议扩展 |
| 公开 Native ABI | 未承诺 | 只有进程协议无法满足且有性能证据时再评估 |


包名就是顶级命名空间：package_name 只能注册 package_name.*，不能覆盖 hhy.*、std.*、核心 callable 或其他包。导入未安装的包会得到 ModuleNotFoundError。


## 20.2 现有官方扩展

| 扩展 | 版本 | 发布时间 | 状态 | 能力 |
| --- | --- | --- | --- | --- |
| database | 0.2.0 | 2026-08-26 | 已发布 | MySQL/PostgreSQL 查询、写入与事务 |
| html | 0.1.0 | 2026-08-27 | 已发布 | Lexbor CSS Selector、文本/属性读取与结构化抽取 |


[数据库扩展使用指南](/zh/learn/database-extension)

安装 database 0.2.0，并完成 MySQL/PostgreSQL 查询、写入与事务。


[HTML 扩展与抓取框架](/zh/learn/html-crawler-framework)

使用 html 0.1.0、URL 规范化、安全 Frontier、去重和 SSRF 防护构建静态 Spider。


## 20.3 在哪里获取扩展

| 来源 | 适合场景 | 地址或操作 |
| --- | --- | --- |
| HHY 官方 Registry | 下载经过签名、按平台区分的官方扩展 | https://registry.hhylang.dev（索引：/index.json；信任根：/root.json） |
| GitHub 源码 | 查看代码、审计变更或自行编译 | https://github.com/hh696-wq/hhy-vm/tree/main/extensions |
| 本地自行编译 | 开发扩展或需要本机依赖组合 | make -C extensions/<name>，然后从本地目录安装 |


```sh
# 从源码构建并安装（示例：html）
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm
make
make -C extensions/html
./build/hhy install ./extensions/html

# 查看已安装扩展
./build/hhy list
```


[查看官方扩展索引 ↗](https://registry.hhylang.dev/index.json)

同一扩展版本按 darwin-arm64、linux-x86_64、linux-arm64 和 windows-x86_64 分发，安装器只选择当前平台 target。


[在 GitHub 查看扩展源码 ↗](https://github.com/hh696-wq/hhy-vm/tree/main/extensions)

包含 sample、html 与 database 的源码、hhy.toml、测试和构建脚本。


{% hint style="info" %}
GitHub Release 暂不作为扩展下载入口。官方 Registry 提供签名分发；GitHub 提供可审计源码。自行编译时必须在目标操作系统和架构上构建，不能把 macOS 二进制改名用于 Linux 或 Windows。
{% endhint %}


## 20.4 安装、查看与移除

```sh
./build/hhy install ./path/to/extension
./build/hhy list
./build/hhy remove package-name
```


| 步骤 | 实际行为 |
| --- | --- |
| install | 读取 hhy.toml；校验包名、作者、requires_hhy、协议、命令和完整性；展示 capability 后由用户确认安装 |
| import / load | 重新校验已安装文件的 SHA-256，启动扩展进程，握手并注册 callable |
| list | 显示已安装包的名称、版本、作者、协议和声明的 capability |
| remove | 删除本地包记录和安装目录；之后 import 会失败 |


{% hint style="info" %}
默认扩展目录是 ~/.hhy/extensions；设置 HHY_EXTENSION_HOME 可为 CI 或测试提供隔离目录。capability 是安装时可审查的声明，不等同于通用操作系统沙箱；第三方扩展仍应按原生可执行文件对待。
{% endhint %}


## 20.5 通用 hhy.toml 清单

```text
[package]
name = "package-name"
version = "0.1.0"
author = "Your Organization"
requires_hhy = ">=1.1,<2.0"

[extension]
kind = "process"
command = "bin/hhy-package"
protocol = "1"

[capabilities]
read = []
write = []
network = []
process = false
```


| 字段 | 开发者约束 |
| --- | --- |
| package.name | 唯一顶级命名空间，只允许小写字母、数字和连字符 |
| package.author | 安装与 list 时展示，明确官方或第三方来源 |
| requires_hhy | 安装器检查 Runtime 版本范围 |
| extension.command | 必须是包内 bin/ 下的可执行文件，不能逃出包目录 |
| extension.protocol | 当前只接受 Protocol 1 |
| capabilities | 声明需要审查的文件、网络和子进程访问范围 |


## 20.6 扩展如何加载

{% hint style="info" %}
本节的交互式图表请在 [hhylang.dev](https://hhylang.dev/zh/learn/extensions-roadmap) 查看。
{% endhint %}


| 阶段 | Runtime 与扩展的职责 |
| --- | --- |
| resolve | Runtime 根据 import package_name 定位已安装包，解析清单并校验命令与完整性 |
| spawn | Runtime 以 --protocol 1 启动独立进程，并建立 stdin/stdout 协议管道 |
| handshake | 双方确认 extension_id 与 protocol_version=1.0 |
| register | 扩展发送一次注册消息；Runtime 验证包命名空间和 contract 后写入 callable registry |
| call | Runtime 发送可序列化参数；request_id 关联 call 与 call_result |
| shutdown | Runtime 发送 shutdown 并回收协议流和子进程 |


{% hint style="info" %}
Protocol 1 是同步、逐次调用协议，不提供 Stream、Opaque handle 或协议级 cancel。
{% endhint %}


## 20.7 扩展作者需要实现什么

| 部分 | 要求 |
| --- | --- |
| 包 | 提供 hhy.toml、包内可执行命令和安装器可验证的 SHA-256 完整性信息 |
| 启动 | 只接受 --protocol 1；协议消息只写 stdout，日志写 stderr |
| handshake | 验证 extension_id 与 protocol_version，并返回匹配身份 |
| register | 恰好发送一次初始注册；名称必须位于包命名空间且 contract 完整 |
| call | 按 request_id 返回 call_result 或结构化 error，不泄露凭据或敏感诊断 |
| shutdown | 幂等释放连接、内存和其他扩展资源 |
| 测试 | 至少覆盖身份不匹配、非法参数、扩展退出、协议错误和资源清理 |


扩展进程不会继承完整宿主环境；Runtime 只通过协议传递脚本显式提供的参数。错误应可定位，但不得包含密码、令牌、完整连接地址或其他敏感信息。
