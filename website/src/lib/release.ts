export const hhyVersion = "1.3.2" as const;
export const hhyVersionLabel = `V${hhyVersion}` as const;
export const hhyVersionTag = `v${hhyVersion}` as const;
export const hhyReleaseUrl = `https://github.com/hh696-wq/hhy-vm/releases/tag/${hhyVersionTag}` as const;
export const hhyCoreCallableCount = 96;

// Version-specific presentation belongs here so a version bump cannot leave
// the homepage title and description pointing at an older release theme.
export const hhyCurrentRelease = {
  zh: {
    title: "Bytecode VM 边界稳定化",
    summary: "固化 Compiler、Verifier、执行准备与共享 Runtime 的内部边界，持续保留 AST 语义 oracle 和真实负载双引擎矩阵。",
    verification: "静态治理阻止 Runtime 绕过版本化 Bytecode 边界；完整套件、故障注入、性能决策和官方 workload 持续生成证据。"
  },
  en: {
    title: "Bytecode VM boundary stabilization",
    summary: "Stabilizes the internal Compiler, Verifier, execution-preparation, and shared Runtime boundary while retaining the AST semantic oracle and real-workload dual-engine matrix.",
    verification: "Static governance prevents Runtime bypass of the versioned Bytecode boundary; the full suite, fault injection, performance decision, and official workloads keep producing evidence."
  }
} as const;
