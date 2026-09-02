export const hhyVersion = "1.3.10" as const;
export const hhyVersionLabel = `V${hhyVersion}` as const;
export const hhyVersionTag = `v${hhyVersion}` as const;
export const hhyReleaseUrl = `https://github.com/hh696-wq/hhy-vm/releases/tag/${hhyVersionTag}` as const;
export const hhyCoreCallableCount = 96;
export const hhyEditorVersions = {
  vscode: "0.2.0",
  sublime: "0.1.0"
} as const;

export const hhyAboutMilestones = {
  zh: [
    { version: "v1.0", title: "语言核心", body: "Pipe、Value、Stream、Error 与核心标准库完成语义冻结。", status: "released" },
    { version: "v1.2", title: "可信分发", body: "签名 Registry、确定性依赖解析、锁定文件、离线安装与安全回滚已经发布。", status: "released" },
    { version: hhyVersionTag, title: "Bytecode 执行基线", body: "Bytecode 已成为默认引擎；特化、Profiler、资源一致性与缓存治理完成验证。", status: "current" },
    { version: "v1.4", title: "真实生态验证", body: "下一阶段聚焦官方模板、CI、运维文档与 3–5 个外部真实案例。", status: "planned" }
  ],
  en: [
    { version: "v1.0", title: "Language core", body: "Pipe, Value, Stream, Error, and the core standard library reached semantic freeze.", status: "released" },
    { version: "v1.2", title: "Trusted distribution", body: "The signed Registry, deterministic dependency resolution, lockfiles, offline installation, and safe rollback have shipped.", status: "released" },
    { version: hhyVersionTag, title: "Bytecode execution baseline", body: "Bytecode is now the default engine, with specialization, profiling, resource consistency, and cache governance verified.", status: "current" },
    { version: "v1.4", title: "Real ecosystem validation", body: "The next stage focuses on official templates, CI, operations documentation, and 3–5 real external cases.", status: "planned" }
  ]
} as const;

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
