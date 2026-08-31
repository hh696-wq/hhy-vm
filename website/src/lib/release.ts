export const hhyVersion = "1.2.0" as const;
export const hhyVersionLabel = `V${hhyVersion}` as const;
export const hhyVersionTag = `v${hhyVersion}` as const;
export const hhyReleaseUrl = `https://github.com/hh696-wq/hhy-vm/releases/tag/${hhyVersionTag}` as const;
export const hhyCoreCallableCount = 96;

// Version-specific presentation belongs here so a version bump cannot leave
// the homepage title and description pointing at an older release theme.
export const hhyCurrentRelease = {
  zh: {
    title: "官方扩展分发与签名",
    summary: "增加命名空间包身份、Ed25519 签名 Registry、确定性传递依赖解析，以及全量验证后提交的事务式安装。",
    verification: "本地签名 fixture 覆盖 dry-run、依赖顺序、索引与 payload 篡改拒绝、失败回滚；四平台 CI 持续验证。"
  },
  en: {
    title: "Official extension distribution and signing",
    summary: "Adds namespaced package identities, an Ed25519-signed Registry, deterministic transitive dependency resolution, and transaction-safe installs committed only after full verification.",
    verification: "Signed local fixtures cover dry runs, dependency order, index and payload tampering, and failure rollback, with continuing four-platform CI verification."
  }
} as const;
