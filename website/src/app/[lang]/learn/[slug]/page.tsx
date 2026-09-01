import type { Metadata } from "next";
import { ArrowRight, Code, Handshake, PuzzlePiece, RocketLaunch, ShieldCheck, TerminalWindow, WarningDiamond } from "@phosphor-icons/react/dist/ssr";
import Image from "next/image";
import { notFound } from "next/navigation";
import type { ElementType } from "react";
import { CodeBlock } from "@/components/code-block";
import { JsonLd } from "@/components/json-ld";
import { LearnLayout } from "@/components/learn-layout";
import { chapterKind, chapters, getChapter } from "@/lib/docs";
import { isLanguage, languages } from "@/lib/i18n";
import { hhyVersion, hhyVersionTag } from "@/lib/release";
import { createMetadata, localizedUrl, siteName } from "@/lib/seo";

function RoadmapVersion({ value }: { value: string }) {
  const [version, ...statuses] = value.split(" · ");
  return <span className="roadmap-version"><b>{version}</b>{statuses.map((status) => {
    const statusKey = status === "已发布" || status === "Released"
      ? "released"
      : status === "测试中" || status === "Testing"
        ? "testing"
        : status === "当前" || status === "Current"
          ? "current"
          : status === "规划" || status === "Planned"
            ? "planned"
            : "conditional";
    return <em data-status={statusKey} key={status}>{status}</em>;
  })}</span>;
}

function ReleaseLineage({ columns, rows, lang }: { columns: string[]; rows: string[][]; lang: "zh" | "en" }) {
  const groups = [
    { label: "v1.0.x", matches: (version: string) => version.startsWith("v1.0.") },
    { label: "v1.1.x", matches: (version: string) => version.startsWith("v1.1.") },
    { label: "v1.2.x", matches: (version: string) => version === "v1.2" || version.startsWith("v1.2.") },
    { label: "v1.3.x", matches: (version: string) => version === "v1.3" || version.startsWith("v1.3.") },
  ];
  return <div className="release-lineage">
    {groups.map((group) => {
      const releases = rows.filter((row) => group.matches(row[0].split(" · ")[0]));
      if (releases.length === 0) return null;
      return <details className="release-lineage-group" key={group.label}>
        <summary>
          <span>{group.label}</span>
          <small>{lang === "zh" ? `${releases.length} 个版本` : `${releases.length} ${releases.length === 1 ? "release" : "releases"}`}</small>
          <span className="release-lineage-toggle" aria-hidden="true">+</span>
        </summary>
        <div className="doc-table-wrap">
          <table className="doc-table">
            <thead><tr>{columns.map((column) => <th key={column}>{column}</th>)}</tr></thead>
            <tbody>{releases.map((row) => <tr key={row[0]}>{row.map((cell, cellIndex) =>
              <td data-label={columns[cellIndex]} key={cellIndex}>{cellIndex === 0 && cell.includes(" · ")
                ? <RoadmapVersion value={cell} />
                : cell}</td>
            )}</tr>)}</tbody>
          </table>
        </div>
      </details>;
    })}
  </div>;
}

export function generateStaticParams() {
  return languages.flatMap((lang) => chapters.map((chapter) => ({ lang, slug: chapter.slug })));
}

export async function generateMetadata({ params }: { params: Promise<{ lang: string; slug: string }> }): Promise<Metadata> {
  const { lang, slug } = await params;
  const chapter = getChapter(slug);
  if (!isLanguage(lang) || !chapter) return {};
  return createMetadata({
    language: lang,
    path: `/learn/${slug}`,
    title: `${chapter.title[lang]} — ${lang === "zh" ? "HHY 语言手册" : "HHY Language Manual"}`,
    description: chapter.summary[lang],
    type: "article",
    keywords: [chapter.title[lang], slug.replaceAll("-", " ")]
  });
}

export default async function ChapterPage({ params }: { params: Promise<{ lang: string; slug: string }> }) {
  const { lang, slug } = await params;
  const chapter = getChapter(slug);
  if (!isLanguage(lang) || !chapter) notFound();

  return (
    <LearnLayout language={lang} chapter={chapter}>
      <JsonLd data={{
        "@context": "https://schema.org",
        "@type": "TechArticle",
        headline: chapter.title[lang],
        description: chapter.summary[lang],
        url: localizedUrl(lang, `/learn/${slug}`),
        inLanguage: lang === "zh" ? "zh-CN" : "en",
        isPartOf: { "@type": "WebSite", name: siteName, url: localizedUrl(lang) },
        author: { "@type": "Organization", name: "HHY Language contributors", url: "https://github.com/hh696-wq/hhy-vm" },
        version: hhyVersion,
        about: ["HHY Language", "system scripting", "Flow pipelines"]
      }} />
      <article className="chapter-article">
        <header className="chapter-hero">
          <p className="eyebrow">{chapterKind(chapter) === "guide"
            ? (lang === "zh" ? `指南 · 第 ${chapter.order} 章` : `Guide · Chapter ${chapter.order}`)
            : chapterKind(chapter) === "project"
              ? (lang === "zh" ? "实战项目 · 已通过自测" : "Project · Self-tested")
            : chapterKind(chapter) === "reference"
              ? (lang === "zh" ? "HHY 参考" : "HHY Reference")
            : chapterKind(chapter) === "extension"
              ? (lang === "zh" ? `HHY 扩展 · 当前版本 v${hhyVersion}` : `HHY Extension · Current version v${hhyVersion}`)
            : chapterKind(chapter) === "report"
              ? (lang === "zh" ? "HHY 语言报告" : "HHY Language Report")
            : chapterKind(chapter) === "roadmap"
              ? (lang === "zh" ? "HHY 路线图" : "HHY Roadmap")
              : (lang === "zh" ? "HHY 工具" : "HHY Tooling")}</p>
          <h1>{chapter.title[lang]}</h1>
          <p className="chapter-summary">{chapter.summary[lang]}</p>
        </header>
        {chapter.sections[lang].map((section, sectionIndex) => (
          <section id={`section-${sectionIndex + 1}`} key={section.title}>
            <h2>{section.title}</h2>
            {section.blocks.map((block, index) => {
              if (block.type === "p") return <p key={index}>{block.text}</p>;
              if (block.type === "note") return <aside className="doc-note" key={index}>{block.text}</aside>;
              if (block.type === "list") return <ul key={index}>{block.items.map((item) => <li key={item}>{item}</li>)}</ul>;
              if (block.type === "table" && slug === "language-vm-roadmap" && (block.columns[0] === "版本" || block.columns[0] === "Release")) {
                return <ReleaseLineage columns={block.columns} rows={block.rows} lang={lang} key={index} />;
              }
              if (block.type === "table") return (
                <div className="doc-table-wrap" key={index}>
                  <table className="doc-table">
                    <thead><tr>{block.columns.map((column) => <th key={column}>{column}</th>)}</tr></thead>
                    <tbody>{block.rows.map((row, rowIndex) => <tr key={rowIndex}>{row.map((cell, cellIndex) => {
                      const isRoadmapVersion = slug === "language-vm-roadmap" && cellIndex === 0 && cell.includes(" · ");
                      const [version, ...statuses] = isRoadmapVersion ? cell.split(" · ") : [cell];
                      return <td data-label={block.columns[cellIndex]} key={cellIndex}>{isRoadmapVersion
                        ? <RoadmapVersion value={[version, ...statuses].join(" · ")} />
                        : cell}</td>;
                    })}</tr>)}</tbody>
                  </table>
                </div>
              );
              if (block.type === "link") return <a className="doc-link-card" href={block.href} target={block.href.startsWith("/") ? undefined : "_blank"} rel={block.href.startsWith("/") ? undefined : "noreferrer"} key={index}><strong>{block.label}</strong><span>{block.description}</span></a>;
              if (block.type === "image") return <figure className={`doc-image ${block.size}`} key={index}><a href={block.src} target="_blank" rel="noreferrer" aria-label={lang === "zh" ? "查看原图" : "View full-size image"}><Image src={block.src} alt={block.alt} width={block.width} height={block.height} sizes={block.size === "medium" ? "(max-width: 560px) calc(100vw - 32px), 480px" : "(max-width: 720px) calc(100vw - 32px), 680px"} /></a><figcaption>{block.caption}</figcaption></figure>;
              if (block.type === "runtime-performance-roadmap") {
                const copy = lang === "zh" ? {
                  eyebrow: `${hhyVersionTag} · 当前执行路径`, title: "AST 解释器性能演进", current: "当前 AST Interpreter",
                  resolver: "AST 预解析 / Resolve Pass", resolverItems: ["参数绑定到 slot", "局部变量绑定到 slot", "标记 global / builtin / closure"],
                  frame: "Lightweight CallFrame", call: "function call", frameCode: "slots[]  ·  parent/env  ·  function",
                  fast: "Local / Param", fastDetail: "slots[index] · 快路径", slow: "Closure / Global / Builtin", slowDetail: "Env lookup · 兼容慢路径",
                  cache: "Identifier Cache", cacheDetail: "无法 slot 化的变量缓存 env depth + binding slot/index",
                  pool: "Frame Pool", reuse: "未逃逸 Frame", reuseDetail: "reset → reuse", escaped: "Closure / Stream 捕获", escapedDetail: "mark escaped · 不复用 · GC managed",
                  profile: "Profiling + GC / Allocation 优化", decision: "Profile 仍显示 AST dispatch 为主要热点？", future: "未来方向", bytecode: "Bytecode VM",
                  caption: "Resolver → Slot → Lightweight Frame → Escape-safe Reuse；保持现有语言语义，不依赖字节码。"
                } : {
                  eyebrow: `${hhyVersionTag} · Current execution path`, title: "AST interpreter performance evolution", current: "Current AST Interpreter",
                  resolver: "AST pre-resolution / Resolve Pass", resolverItems: ["Bind parameters to slots", "Bind locals to slots", "Mark globals / builtins / closures"],
                  frame: "Lightweight CallFrame", call: "function call", frameCode: "slots[]  ·  parent/env  ·  function",
                  fast: "Local / Param", fastDetail: "slots[index] · fast path", slow: "Closure / Global / Builtin", slowDetail: "Env lookup · compatibility path",
                  cache: "Identifier Cache", cacheDetail: "Cache env depth + binding slot/index for values that cannot use static slots",
                  pool: "Frame Pool", reuse: "Non-escaped frame", reuseDetail: "reset → reuse", escaped: "Captured by Closure / Stream", escapedDetail: "mark escaped · no reuse · GC managed",
                  profile: "Profiling + GC / Allocation optimization", decision: "Does profiling show AST dispatch as the dominant hotspot?", future: "Future direction", bytecode: "Bytecode VM",
                  caption: "Resolver → Slot → Lightweight Frame → escape-safe reuse, preserving language semantics without requiring bytecode."
                };
                return <figure className="runtime-performance-roadmap" key={index}>
                  <header><span>{copy.eyebrow}</span><strong>{copy.title}</strong></header>
                  <div className="runtime-roadmap-flow">
                    <div className="runtime-roadmap-source">{copy.current}</div><i aria-hidden>↓</i>
                    <article className="runtime-roadmap-stage"><b>01</b><div><strong>{copy.resolver}</strong><ul>{copy.resolverItems.map((item) => <li key={item}>{item}</li>)}</ul></div></article><i aria-hidden>↓</i>
                    <article className="runtime-roadmap-stage frame"><b>02</b><div><strong>{copy.frame}</strong><small>{copy.call}</small><code>{copy.frameCode}</code><div className="runtime-frame-paths"><span><em>{copy.fast}</em>{copy.fastDetail}</span><span><em>{copy.slow}</em>{copy.slowDetail}</span></div></div></article><i aria-hidden>↓</i>
                    <article className="runtime-roadmap-stage"><b>03</b><div><strong>{copy.cache}</strong><p>{copy.cacheDetail}</p></div></article><i aria-hidden>↓</i>
                    <article className="runtime-roadmap-stage pool"><b>04</b><div><strong>{copy.pool}</strong><div className="runtime-frame-paths"><span><em>{copy.reuse}</em>{copy.reuseDetail}</span><span><em>{copy.escaped}</em>{copy.escapedDetail}</span></div></div></article><i aria-hidden>↓</i>
                    <article className="runtime-roadmap-stage"><b>05</b><div><strong>{copy.profile}</strong><p>{copy.decision}</p></div></article><i className="future" aria-hidden>↓</i>
                    <div className="runtime-roadmap-future"><span>{copy.future}</span><strong>06 · {copy.bytecode}</strong></div>
                  </div>
                  <figcaption>{copy.caption}</figcaption>
                </figure>;
              }
              if (block.type === "evolution-roadmap") {
                const released: Array<{ version: string; date: string; title: string; detail: string; icon: ElementType }> = lang === "zh"
                  ? [
                    { version: "v1.0.0", date: "2026-08-25", title: "核心语义冻结", detail: "Pipe / Value / Stream / Error、94 个核心 callable、三平台发布验证", icon: Code },
                    { version: "v1.1.0", date: "2026-08-26", title: "本地进程扩展", detail: "install/list/remove、Protocol 1、database 0.2.0、三平台发布验证", icon: PuzzlePiece },
                    { version: "v1.1.1", date: "2026-08-27", title: "性能与临界稳定性", detail: "hhy profile、解释器热点优化、资源临界值压力测试与稳定错误", icon: ShieldCheck },
                    { version: "v1.1.2", date: "2026-08-27", title: "HTML 扩展与静态采集", detail: "CSS Selector、结构化抽取、my-crawler 与真实抓取验收", icon: ShieldCheck },
                    { version: "v1.1.3", date: "2026-08-28", title: "Runtime 正确性与性能加固", detail: "GC 根生命周期、哈希索引、进程诊断、Profiler 质量标记与三平台验证", icon: ShieldCheck },
                    { version: "v1.1.4", date: "2026-08-28", title: "安全静态 Spider", detail: "URL 规范化、链接发现、Frontier、抓取边界、指纹去重与连接级 SSRF 防护", icon: ShieldCheck },
                    { version: "v1.1.5", date: "2026-08-30", title: "可恢复 Spider 与浏览器渲染", detail: "持久 Frontier、断点恢复、流式落盘、可选 Playwright 与 Windows MSYS2 构建证据", icon: ShieldCheck },
                    { version: "v1.1.6", date: "2026-08-31", title: "稳定基线与测试治理", detail: "宿主能力探测、分层 CI、机器可读性能基线与发布一致性门禁", icon: ShieldCheck },
                    { version: "v1.1.7", date: "2026-08-31", title: "诊断与编辑器基线", detail: "版本化 JSON diagnostics、Contract Registry JSON、LSP 与 VS Code 编辑闭环", icon: ShieldCheck },
                    { version: "v1.1.8", date: "2026-08-31", title: "Runtime 渐进治理", detail: "首个模块边界、内部所有权规则、GC/sanitizer 与性能回归门禁", icon: ShieldCheck },
                    { version: "v1.2.0", date: "2026-08-31", title: "官方扩展分发与签名", detail: "Ed25519 签名 Registry、传递依赖解析、dry-run 与事务式安装", icon: Handshake },
                    { version: "v1.2.1", date: "2026-09-01", title: "锁定、离线与安全回滚", detail: "Lockfile、离线缓存、可复现安装、事务式升级与回滚", icon: Handshake },
                    { version: "v1.2.2", date: "2026-09-01", title: "官方 HTML 复杂扩展验证", detail: "批量抽取、可观察截断、结构化错误与四平台发行", icon: Handshake },
                    { version: "v1.3.0-alpha", date: "2026-09-01", title: "可验证 Bytecode 编译器骨架", detail: "Chunk、Opcode、常量池、源码位置、AST compiler、Verifier 与反汇编；AST 仍为默认引擎", icon: Code },
                    { version: "v1.3.0", date: "待统一发布", title: "可选 Bytecode 正式执行路径", detail: "完整双引擎套件、Profiler、HHY Stack trace 与故障注入；性能门禁保持 AST 默认", icon: Code },
                    { version: "v1.3.1", date: "待统一发布", title: "真实负载兼容加固", detail: "官方 workload 双引擎矩阵、能力探测与机器可读证据", icon: ShieldCheck },
                    { version: "v1.3.2", date: "待统一发布", title: "VM 内部边界稳定化", detail: "版本化 Bytecode Runtime 边界、静态治理与持续 AST oracle", icon: ShieldCheck }
                  ]
                  : [
                    { version: "v1.0.0", date: "2026-08-25", title: "Core semantics frozen", detail: "Pipe / Value / Stream / Error, 94 core callables, and three-platform release evidence", icon: Code },
                    { version: "v1.1.0", date: "2026-08-26", title: "Local process extensions", detail: "install/list/remove, Protocol 1, database 0.2.0, and three-platform release evidence", icon: PuzzlePiece },
                    { version: "v1.1.1", date: "2026-08-27", title: "Performance and boundary stability", detail: "hhy profile, interpreter hotspot optimization, resource-boundary stress tests, and stable errors", icon: ShieldCheck },
                    { version: "v1.1.2", date: "2026-08-27", title: "HTML extension and static collection", detail: "CSS selectors, structured extraction, my-crawler, and a real crawl acceptance run", icon: ShieldCheck },
                    { version: "v1.1.3", date: "2026-08-28", title: "Runtime correctness and performance hardening", detail: "GC root lifetimes, hash indexes, process diagnostics, profiler quality metadata, and three-platform evidence", icon: ShieldCheck },
                    { version: "v1.1.4", date: "2026-08-28", title: "Safe static spider", detail: "URL normalization, link discovery, frontier limits, fingerprint deduplication, and connection-level SSRF protection", icon: ShieldCheck },
                    { version: "v1.1.5", date: "2026-08-30", title: "Resumable spider and browser rendering", detail: "Persistent frontier, resume, streamed files, optional Playwright, and Windows MSYS2 build evidence", icon: ShieldCheck },
                    { version: "v1.1.6", date: "2026-08-31", title: "Stable engineering baseline", detail: "Host capability probes, layered CI, machine-readable performance baselines, and release consistency gates", icon: ShieldCheck },
                    { version: "v1.1.7", date: "2026-08-31", title: "Diagnostics and editor baseline", detail: "Versioned JSON diagnostics, Contract Registry JSON, LSP, and a VS Code editing loop", icon: ShieldCheck },
                    { version: "v1.1.8", date: "2026-08-31", title: "Gradual Runtime governance", detail: "First module boundary, internal ownership rules, GC/sanitizer, and a performance-regression gate", icon: ShieldCheck },
                    { version: "v1.2.0", date: "2026-08-31", title: "Official extension distribution and signing", detail: "Ed25519-signed Registry, transitive resolution, dry runs, and transaction-safe installs", icon: Handshake },
                    { version: "v1.2.1", date: "2026-09-01", title: "Locking, offline installs, and safe rollback", detail: "Lockfiles, offline caches, reproducible installs, transactional upgrades, and rollback", icon: Handshake },
                    { version: "v1.2.2", date: "2026-09-01", title: "Official HTML complex-extension validation", detail: "Batch extraction, observable truncation, structured errors, and four-platform releases", icon: Handshake },
                    { version: "v1.3.0-alpha", date: "2026-09-01", title: "Verifiable Bytecode compiler skeleton", detail: "Chunks, opcodes, a constant pool, source locations, an AST compiler, verifier, and disassembler; AST remains the default", icon: Code },
                    { version: "v1.3.0", date: "Coordinated release pending", title: "Opt-in production Bytecode path", detail: "Full dual-engine suite, profiling, HHY stack traces, and fault injection; the performance gate retains AST as default", icon: Code },
                    { version: "v1.3.1", date: "Coordinated release pending", title: "Real-workload compatibility hardening", detail: "Official workload dual-engine matrix, capability probes, and machine-readable evidence", icon: ShieldCheck },
                    { version: "v1.3.2", date: "Coordinated release pending", title: "VM internal-boundary stabilization", detail: "Versioned Bytecode Runtime boundary, static governance, and a continuous AST oracle", icon: ShieldCheck }
                  ];
                const releases: Array<{ version: string; window: string; title: string; items: string[]; icon: ElementType }> = lang === "zh"
                  ? [
                    { version: "v1.4", window: "v1.3 稳定后", title: "旗舰场景与外部采用", items: ["官方项目模板与 CI", "运维和故障诊断文档", "3–5 个外部真实案例"], icon: Handshake },
                    { version: "v2.0", window: "生态证据充分后", title: "生态开放与 ABI 决策", items: ["以真实集成测量进程协议边界", "评估 embedding / FFI", "仅在必要时发布 Native ABI"], icon: RocketLaunch }
                  ]
                  : [
                    { version: "v1.4", window: "After v1.3 stabilizes", title: "Flagship scenarios and external adoption", items: ["Official project templates and CI", "Operations and troubleshooting documentation", "3–5 real external cases"], icon: Handshake },
                    { version: "v2.0", window: "After sufficient ecosystem evidence", title: "Ecosystem and ABI decision", items: ["Measure process-protocol limits with real integrations", "Evaluate embedding / FFI", "Publish a Native ABI only if necessary"], icon: RocketLaunch }
                  ];
                const principles = lang === "zh"
                  ? ["先冻结语义，再开放扩展", "先可用、可测，再做高性能", "扩展通过协议接入，不另造语义", "ABI 只在 Runtime 稳定后评估"]
                  : ["Freeze semantics before opening extensions", "Make it usable and measurable before fast", "Extend through protocol, not a second language model", "Evaluate ABI only after Runtime stability"];
                const history = released.slice(0, -1);
                const current = released.at(-1)!;
                const CurrentIcon = current.icon;
                return (
                  <figure className="evolution-roadmap" key={index}>
                    <header><strong>{lang === "zh" ? "语言 / VM 演进路线图" : "Language / VM Evolution Roadmap"}</strong><span>{lang === "zh" ? `当前 v${hhyVersion} · 后续只保留两个方向` : `Current v${hhyVersion} · only two future directions`}</span></header>
                    <div className="evolution-release-stage">
                      <details className="evolution-history">
                        <summary><span>{lang === "zh" ? "已发布版本" : "Released history"}</span><small>{lang === "zh" ? `${history.length} 个版本 · 点击展开` : `${history.length} releases · expand`}</small><b aria-hidden>+</b></summary>
                        <div className="evolution-history-grid">
                          {history.map(({ version, date, title, detail, icon: Icon }) => <article key={version}><Icon size={38} weight="duotone" aria-hidden /><div><span><b>{version}</b><time>{date}</time><em>{lang === "zh" ? "已发布" : "Released"}</em></span><h3>{title}</h3><p>{detail}</p></div></article>)}
                        </div>
                      </details>
                      <article className="evolution-current">
                        <CurrentIcon size={48} weight="duotone" aria-hidden />
                        <div className="evolution-current-title"><span><b>{current.version}</b><em>{lang === "zh" ? "当前 · 已验证" : "Current · Verified"}</em></span><time>{current.date}</time><h3>{current.title}</h3></div>
                        <p>{current.detail}</p>
                      </article>
                    </div>
                    <div className="evolution-future-label"><span>{lang === "zh" ? "未来两个版本" : "Two future releases"}</span><small>{lang === "zh" ? "按验收门槛依次进入" : "Enter sequentially through acceptance gates"}</small></div>
                    <div className="evolution-track">
                      {releases.map(({ version, window, title, items, icon: Icon }, releaseIndex) => (
                        <article key={version}>
                          <span className="evolution-order">{String(releaseIndex + 1).padStart(2, "0")}</span>
                          <Icon size={56} weight="duotone" aria-hidden />
                          <div>
                            <span className="evolution-meta"><b>{version}</b><time>{window}</time><em>{lang === "zh" ? "规划" : "Planned"}</em></span>
                            <h3>{title}</h3>
                            <ul>{items.map((item) => <li key={item}>{item}</li>)}</ul>
                          </div>
                        </article>
                      ))}
                    </div>
                    <div className="evolution-principles"><strong><ShieldCheck size={30} weight="duotone" />{lang === "zh" ? "演进原则" : "Evolution principles"}</strong>{principles.map((principle, principleIndex) => <span key={principle}><b>{principleIndex + 1}</b>{principle}</span>)}</div>
                    <figcaption>{lang === "zh" ? "演进顺序：v1.1.8 Runtime 治理 → v1.2.0 签名分发 → v1.2.1 锁定与回滚 → 生态 ABI 决策" : "Evolution order: v1.1.8 Runtime governance → v1.2.0 signed distribution → v1.2.1 locking and rollback → ecosystem ABI decision"}</figcaption>
                  </figure>
                );
              }
              if (block.type === "extension-flow") {
                const steps: Array<[string, string, string, ElementType]> = lang === "zh"
                  ? [
                    ["HHY 脚本", "导入扩展包", "import package_name", Code],
                    ["Runtime", "验证并初始化", "校验清单与 SHA-256", ShieldCheck],
                    ["扩展进程", "启动隔离进程", "bin/hhy-package\n--protocol 1", RocketLaunch],
                    ["协议注册", "注册可调用能力", "handshake\nregister callables", PuzzlePiece],
                    ["能力实现", "处理协议参数", "Null / Bool / Number\nString / List / Map", PuzzlePiece],
                    ["调用执行", "返回结果或错误", "call_result\n或结构化 Error", TerminalWindow]
                  ]
                  : [
                    ["HHY script", "Import a package", "import package_name", Code],
                    ["Runtime", "Validate and initialize", "Verify manifest and SHA-256", ShieldCheck],
                    ["Extension process", "Start isolated process", "bin/hhy-package\n--protocol 1", RocketLaunch],
                    ["Protocol registration", "Register callable capability", "handshake\nregister callables", PuzzlePiece],
                    ["Implementation", "Handle protocol values", "Null / Bool / Number\nString / List / Map", PuzzlePiece],
                    ["Call execution", "Return result or error", "call_result\nor structured Error", TerminalWindow]
                  ];
                const guarantees: Array<[string, string, ElementType]> = lang === "zh"
                  ? [["完整性验证", "安装与加载时校验 SHA-256", ShieldCheck], ["标准协议", "Protocol 1 统一互操作", Handshake], ["能力注册", "包命名空间内动态注册", PuzzlePiece], ["统一响应", "结构化结果与错误", TerminalWindow]]
                  : [["Integrity", "Verify SHA-256 on install and load", ShieldCheck], ["Standard protocol", "Protocol 1 interoperability", Handshake], ["Capability registry", "Dynamic registration inside the package namespace", PuzzlePiece], ["Unified response", "Structured results and errors", TerminalWindow]];
                return (
                  <figure className="extension-flow" key={index}>
                    <header className="extension-flow-header">
                      <div><strong>{lang === "zh" ? "扩展如何加载" : "How an extension loads"}</strong><span>{lang === "zh" ? "HHY 扩展加载架构图" : "HHY extension loading architecture"}</span></div>
                      <aside><span><ArrowRight size={22} weight="bold" />{lang === "zh" ? "流程方向" : "Flow"}</span><span className="flow-legend-error"><WarningDiamond size={20} weight="fill" />{lang === "zh" ? "错误返回" : "Error return"}</span><span><b>N</b>{lang === "zh" ? "执行顺序" : "Order"}</span></aside>
                    </header>
                    <div className="extension-flow-track">
                      {steps.map(([title, subtitle, detail, Icon], stepIndex) => (
                        <div className="extension-flow-item" key={String(title)}>
                          <article className="extension-flow-step">
                            <span className="extension-flow-order">{stepIndex + 1}</span>
                            <strong>{String(title)}</strong>
                            <small>{String(subtitle)}</small>
                            <Icon size={58} weight="duotone" aria-hidden />
                            <code>{String(detail)}</code>
                          </article>
                          {stepIndex < steps.length - 1 ? <ArrowRight className="extension-flow-arrow" size={30} weight="bold" aria-hidden /> : null}
                        </div>
                      ))}
                      <div className="extension-error-return"><WarningDiamond size={22} weight="fill" /><strong>{lang === "zh" ? "结构化错误返回" : "Structured error return"}</strong><span>{lang === "zh" ? "任一协议或调用失败 → Runtime Error → HHY 脚本" : "Protocol or call failure → Runtime Error → HHY script"}</span></div>
                    </div>
                    <div className="extension-flow-guarantees">{guarantees.map(([title, detail, Icon]) => <article key={String(title)}><Icon size={38} weight="duotone" aria-hidden /><div><strong>{String(title)}</strong><span>{String(detail)}</span></div></article>)}</div>
                    <figcaption>{lang === "zh" ? `${hhyVersion} 扩展加载流程 · 独立进程隔离 · Runtime 负责校验、协议分发和 HHY 值转换` : `${hhyVersion} extension load path · isolated process · Runtime owns validation, protocol dispatch, and HHY value conversion`}</figcaption>
                  </figure>
                );
              }
              if (block.type === "api") return (
                <div className="api-reference" key={index}>
                  <nav className="api-index" aria-label={lang === "zh" ? "本节函数索引" : "Function index"}>
                    {block.entries.map((entry) => <a href={`#fn-${entry.name.replaceAll(".", "-")}`} key={entry.name}>{entry.name}</a>)}
                  </nav>
                  <div className="api-entries">
                    {block.entries.map((entry) => (
                      <article className="api-entry" id={`fn-${entry.name.replaceAll(".", "-")}`} key={entry.name}>
                        <h3><span>{lang === "zh" ? "函数" : "func"}</span>{entry.name}<a href={`#fn-${entry.name.replaceAll(".", "-")}`} aria-label={lang === "zh" ? `链接到 ${entry.name}` : `Link to ${entry.name}`}>#</a></h3>
                        <pre><code>{entry.signature}</code></pre>
                        <p>{entry.description}</p>
                      </article>
                    ))}
                  </div>
                </div>
              );
              if (block.type === "terminal") return (
                <figure className="terminal-shot" key={index}>
                  <figcaption><span /><span /><span /><strong>{lang === "zh" ? "实际运行结果" : "Actual run"}</strong></figcaption>
                  <div className="terminal-command"><span>$</span>{block.command}</div>
                  <pre>{block.output}</pre>
                </figure>
              );
              if (block.type === "terminal-card") return (
                <figure className="terminal-card" key={index}>
                  <div className="terminal-card-window">
                    <figcaption><span /><span /><span /><strong>{block.title}</strong></figcaption>
                    <div className="terminal-card-command"><span>$</span>{block.command}</div>
                    <pre>{block.output}</pre>
                  </div>
                  <p>{block.caption}</p>
                </figure>
              );
              return <CodeBlock code={block.code} language={block.language} filename={block.filename ?? (block.language === "hhy" ? "example.hhy" : undefined)} locale={lang} compact key={index} />;
            })}
          </section>
        ))}
      </article>
    </LearnLayout>
  );
}
