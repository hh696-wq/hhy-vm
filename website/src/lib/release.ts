export const hhyVersion = "1.3.0" as const;
export const hhyVersionLabel = `V${hhyVersion}` as const;
export const hhyVersionTag = `v${hhyVersion}` as const;
export const hhyReleaseUrl = `https://github.com/hh696-wq/hhy-vm/releases/tag/${hhyVersionTag}` as const;
export const hhyCoreCallableCount = 96;

// Version-specific presentation belongs here so a version bump cannot leave
// the homepage title and description pointing at an older release theme.
export const hhyCurrentRelease = {
  zh: {
    title: "可选择的 Bytecode 执行引擎",
    summary: "正式提供经过 Verifier 的 Bytecode 执行入口、双引擎语义对照、Profiler 与 HHY Stack trace；AST 因性能门槛未全部通过而继续作为默认引擎。",
    verification: "AST 与 Bytecode 完整测试套件分别通过；非法 Bytecode 失败关闭；默认切换由机器可读性能门禁决定。"
  },
  en: {
    title: "Opt-in Bytecode execution engine",
    summary: "Ships a verified Bytecode execution entry, dual-engine semantic conformance, profiling, and HHY stack traces. AST remains the default because not every performance switch gate passed.",
    verification: "The complete suite passes independently under AST and Bytecode; malformed Bytecode fails closed; a machine-readable performance gate controls any default switch."
  }
} as const;
