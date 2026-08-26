import { hhyVersionLabel } from "./release";

export const languages = ["zh", "en"] as const;

export type Language = (typeof languages)[number];

export function isLanguage(value: string): value is Language {
  return languages.includes(value as Language);
}

export function otherLanguage(language: Language): Language {
  return language === "zh" ? "en" : "zh";
}

export const ui = {
  zh: {
    docs: "文档",
    learn: "学习",
    tutorial: "教程",
    spec: "规范",
    github: "GitHub",
    download: `下载 ${hhyVersionLabel}`,
    version: `HHY Language ${hhyVersionLabel}`,
    headline: "Pipe Everything.",
    subtitle: "一门面向系统自动化、以数据流为核心的脚本语言。",
    solo: "独立构建，为流而生。",
    getStarted: "开始学习",
    readSpec: "阅读规范",
    copy: "复制",
    copied: "已复制",
    what: "什么是 HHY？",
    whatBody: "HHY 用同一种管道模型连接文件、进程、网络请求和结构化数据。",
    flow: "Flow",
    flowBody: "从数据流出发，而不是从循环出发。",
    pipe: "Pipe",
    pipeBody: "把步骤组合成从左到右可读的管道。",
    system: "System",
    systemBody: "原生访问文件、进程、网络与系统信息。",
    simple: "清晰语法",
    simpleBody: "确定的 grammar、类型规则和执行语义，不依赖 AI。",
    units: "原生单位",
    unitsBody: "大小、时间和百分比是运行时可检查的一等类型。",
    safe: "可预测执行",
    safeBody: "不可变集合、有界并发、取消、资源限制和结构化错误。",
    learnTitle: "学习 HHY",
    learnBody: "从安装到编写真实系统脚本，沿着清晰路径逐步掌握。",
    allTutorials: "查看完整手册",
    stable: `${hhyVersionLabel} 已发布并通过三平台公开验证`,
    stableBody: "macOS arm64、Linux arm64、Linux x86_64；包含首个稳定的本地进程扩展工作流。",
    footerLine: "一门以 Flow 为核心的系统脚本语言。"
  },
  en: {
    docs: "Docs",
    learn: "Learn",
    tutorial: "Tutorial",
    spec: "Spec",
    github: "GitHub",
    download: `Download ${hhyVersionLabel}`,
    version: `HHY Language ${hhyVersionLabel}`,
    headline: "Pipe Everything.",
    subtitle: "A flow-first scripting language for system automation.",
    solo: "Built solo. Designed to flow.",
    getStarted: "Get Started",
    readSpec: "Read the Spec",
    copy: "Copy",
    copied: "Copied",
    what: "What is HHY?",
    whatBody: "HHY unifies files, processes, network requests, and structured data through one pipeline model.",
    flow: "Flow",
    flowBody: "Think in flows, not loops.",
    pipe: "Pipe",
    pipeBody: "Compose steps into pipelines that read from left to right.",
    system: "System",
    systemBody: "Native access to files, processes, networks, and system data.",
    simple: "Clear syntax",
    simpleBody: "Deterministic grammar, type rules, and semantics—with no AI dependency.",
    units: "Native units",
    unitsBody: "Size, time, and percent are first-class, runtime-checked values.",
    safe: "Predictable execution",
    safeBody: "Immutable collections, bounded concurrency, cancellation, limits, and structured errors.",
    learnTitle: "Learn HHY",
    learnBody: "Follow a focused path from installation to real system scripts.",
    allTutorials: "View the full manual",
    stable: `${hhyVersionLabel} is released and verified on three platforms`,
    stableBody: "macOS arm64, Linux arm64, and Linux x86_64; includes the first stable local process-extension workflow.",
    footerLine: "A flow-first scripting language for system automation."
  }
} as const;
