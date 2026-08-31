# HHY Signed Extension Registry v1 / HHY 签名扩展仓库 v1

> Status: v1.2.0 implementation contract. This is an official-package trust model, not an open third-party publishing service.

## 中文

HHY v1.2.0 在原有本地目录安装之外，增加静态、可签名的官方 Registry。Registry 可以来自源码仓库、CI artifact 或以后部署的 HTTPS 静态站点；首个实现不要求配置域名。

### 信任与包身份

- 包坐标固定为 `namespace/name`，例如 `official/html`；Runtime 名称仍为 `html`。
- `root.json` 是显式传入的 Ed25519 信任锚，包含 schema version、key id 和 32-byte 公钥。
- `index.json` 整体签名；每个包描述也独立签名。签名覆盖身份、版本、Runtime 名称、依赖、来源目录以及完整文件 SHA-256 清单。
- 客户端先验证索引，再解析完整依赖图，最后验证所有 payload；任一步失败均不安装。
- v1.2.0 仅信任官方根。第三方发布者授权、密钥轮换和撤销属于开放发布前的后续协议。

### 依赖与事务

- 支持精确版本、`>=x.y.z`、`^x.y.z` 和 `*`；同一索引快照中选择满足条件的最高版本。
- 传递依赖按依赖优先顺序确定性输出；循环、缺失版本和版本冲突都会拒绝。
- `--dry-run` 完成验签、解析和 payload 校验，但不创建扩展目录。
- 每个包先复制到扩展目录同文件系统的隐藏 staging 目录，经 SHA-256 复核后原子 rename。
- 安装图中途失败时移除本事务已安装的包，不覆盖既有包。显式升级、lockfile、离线缓存和用户可操作的历史回滚属于 v1.2.1。

```sh
hhy install --dry-run --registry ./registry-snapshot \
  --trust-root ./registry-snapshot/root.json official/sample
hhy install --yes --registry ./registry-snapshot \
  --trust-root ./registry-snapshot/root.json official/sample
```

测试通过临时 Ed25519 密钥生成 fixture，并覆盖依赖顺序、零副作用 dry-run、索引篡改和 payload 篡改拒绝。私钥绝不能进入正式 Registry 或客户端。

宝塔静态站点部署包可在项目根目录执行 `make registry-package` 生成。产物位于
`build/registry/hhy-registry-bt-<version>.tar.gz`，可直接上传并解压到站点根目录；
首次运行生成的正式签名私钥保存在 Git 忽略的 `.hhy-private/`，不会进入部署包。

## English

HHY v1.2.0 adds a static, signed official Registry while preserving local-path development installs. A snapshot may live in a repository, a CI artifact, or later on a static HTTPS origin; the first implementation does not require a domain.

- Coordinates use `namespace/name`; the process-extension runtime name remains separate.
- An explicitly supplied `root.json` is the Ed25519 trust anchor.
- The index and every package descriptor are signed, binding identity, version, dependencies, source, and the complete SHA-256 file manifest.
- Exact, `>=`, caret, and wildcard constraints resolve deterministically with dependencies before dependants. Cycles, conflicts, and missing versions fail closed.
- `--dry-run` verifies the full graph without filesystem changes.
- Packages are verified in same-filesystem staging directories and committed by atomic rename. A graph-level failure removes packages added by that transaction and never replaces an existing package.

Lockfiles, offline caches, explicit upgrades, and user-visible version rollback remain v1.2.1 work. Open third-party publishing, delegated publisher keys, rotation, and revocation are intentionally outside this official-only v1 schema.

Run `make registry-package` to create a BT-ready archive under `build/registry/`.
The first run creates the persistent signing key under the Git-ignored
`.hhy-private/` directory; that key is never included in the deployment archive.
