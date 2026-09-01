export const hhyVersion = "1.3.6" as const;
export const hhyVersionLabel = `V${hhyVersion}` as const;
export const hhyVersionTag = `v${hhyVersion}` as const;
export const hhyReleaseUrl = `https://github.com/hh696-wq/hhy-vm/releases/tag/${hhyVersionTag}` as const;
export const hhyCoreCallableCount = 96;

// Version-specific presentation belongs here so a version bump cannot leave
// the homepage title and description pointing at an older release theme.
export const hhyCurrentRelease = {
  zh: {
    title: "Bytecode 默认执行引擎",
    summary: "run、profile 与脚本简写默认使用经 Compiler/Verifier 验证的 Bytecode 路径，同时保留 AST 语义 oracle 与显式回退。",
    verification: "完整双引擎套件、故障注入、性能证据、真实 workload 与四平台发行持续验证默认路径和 AST 回退。"
  },
  en: {
    title: "Bytecode default execution engine",
    summary: "run, profile, and script shorthand default to the Compiler/Verifier-validated Bytecode path while retaining the AST semantic oracle and explicit fallback.",
    verification: "The dual-engine suite, fault injection, performance evidence, real workloads, and four-platform releases continuously verify the default path and AST fallback."
  }
} as const;
