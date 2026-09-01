export const hhyVersion = "1.3.10" as const;
export const hhyVersionLabel = `V${hhyVersion}` as const;
export const hhyVersionTag = `v${hhyVersion}` as const;
export const hhyReleaseUrl = `https://github.com/hh696-wq/hhy-vm/releases/tag/${hhyVersionTag}` as const;
export const hhyCoreCallableCount = 96;

// Version-specific presentation belongs here so a version bump cannot leave
// the homepage title and description pointing at an older release theme.
export const hhyCurrentRelease = {
  zh: {
    title: "Bytecode 缓存证据治理",
    summary: "真实冷进程测量未达到缓存准入门槛，因此不引入进程或磁盘缓存，也不接受外部预编译 Bytecode。",
    verification: "机器可读决策、完整未来指纹要求、Verifier 门禁与未验证产物拒绝测试持续约束缓存攻击面。"
  },
  en: {
    title: "Evidence-gated Bytecode caching",
    summary: "Measured cold-process costs did not meet cache admission thresholds, so no process or disk cache is introduced and external precompiled Bytecode remains rejected.",
    verification: "A machine-readable decision, complete future fingerprint requirements, Verifier gates, and unverified-artifact rejection tests constrain the cache attack surface."
  }
} as const;
