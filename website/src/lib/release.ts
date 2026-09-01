export const hhyVersion = "1.3.1" as const;
export const hhyVersionLabel = `V${hhyVersion}` as const;
export const hhyVersionTag = `v${hhyVersion}` as const;
export const hhyReleaseUrl = `https://github.com/hh696-wq/hhy-vm/releases/tag/${hhyVersionTag}` as const;
export const hhyCoreCallableCount = 96;

// Version-specific presentation belongs here so a version bump cannot leave
// the homepage title and description pointing at an older release theme.
export const hhyCurrentRelease = {
  zh: {
    title: "Bytecode 真实负载兼容加固",
    summary: "在正式 Bytecode 可选引擎上增加官方真实项目的双引擎兼容矩阵，并持续保留 AST 默认与语义基准。",
    verification: "本地文件治理负载在 AST/Bytecode 下通过；具备 loopback 能力的三平台 CI 继续验证 ETL、FlowGuard、采集器、Crawler 与 SiteGraph。"
  },
  en: {
    title: "Bytecode real-workload compatibility hardening",
    summary: "Adds a dual-engine compatibility matrix for official real projects on top of the opt-in Bytecode engine while retaining AST as the default and semantic oracle.",
    verification: "The local asset-governance workload passes both engines; three-platform CI with loopback support validates ETL, FlowGuard, collectors, Crawler, and SiteGraph."
  }
} as const;
