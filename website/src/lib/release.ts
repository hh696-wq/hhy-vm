export const hhyVersion = "1.2.1" as const;
export const hhyVersionLabel = `V${hhyVersion}` as const;
export const hhyVersionTag = `v${hhyVersion}` as const;
export const hhyReleaseUrl = `https://github.com/hh696-wq/hhy-vm/releases/tag/${hhyVersionTag}` as const;
export const hhyCoreCallableCount = 96;

// Version-specific presentation belongs here so a version bump cannot leave
// the homepage title and description pointing at an older release theme.
export const hhyCurrentRelease = {
  zh: {
    title: "锁定、离线与安全回滚",
    summary: "增加精确 lockfile、内容寻址离线缓存、可复现安装，以及保留已验证旧环境的事务式升级与回滚。",
    verification: "签名 fixture 覆盖索引漂移、缓存损坏、离线重建、失败升级和显式回滚；三系统 CI 作为最终发布门禁。"
  },
  en: {
    title: "Locking, offline installs, and safe rollback",
    summary: "Adds exact lockfiles, content-addressed offline caches, reproducible installs, and transactional upgrades and rollbacks that preserve the last verified environment.",
    verification: "Signed fixtures cover index drift, damaged caches, offline rebuilds, failed upgrades, and explicit rollback, with Linux, macOS, and Windows CI as the final release gate."
  }
} as const;
