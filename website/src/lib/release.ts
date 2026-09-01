export const hhyVersion = "1.3.0-alpha" as const;
export const hhyVersionLabel = `V${hhyVersion}` as const;
export const hhyVersionTag = `v${hhyVersion}` as const;
export const hhyReleaseUrl = `https://github.com/hh696-wq/hhy-vm/releases/tag/${hhyVersionTag}` as const;
export const hhyCoreCallableCount = 96;

// Version-specific presentation belongs here so a version bump cannot leave
// the homepage title and description pointing at an older release theme.
export const hhyCurrentRelease = {
  zh: {
    title: "可验证 Bytecode 编译器骨架",
    summary: "增加内部 Chunk、Opcode、常量池、源码位置、AST compiler、Verifier 和反汇编，同时保留 AST 默认执行路径。",
    verification: "所有合法示例编译为通过 Verifier 的 Bytecode；非法 opcode、常量越界、结构损坏和 HALT 错误稳定拒绝。"
  },
  en: {
    title: "Verifiable Bytecode compiler skeleton",
    summary: "Adds internal chunks, opcodes, a constant pool, source locations, an AST compiler, a verifier, and a disassembler while retaining the AST default execution path.",
    verification: "Every valid example compiles to verified Bytecode; invalid opcodes, constant bounds, malformed structure, and HALT errors fail closed."
  }
} as const;
