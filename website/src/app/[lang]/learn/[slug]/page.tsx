import type { Metadata } from "next";
import { ArrowRight, Code, Database, Handshake, PuzzlePiece, RocketLaunch, ShieldCheck, TerminalWindow, WarningDiamond } from "@phosphor-icons/react/dist/ssr";
import Image from "next/image";
import { notFound } from "next/navigation";
import type { ElementType } from "react";
import { CodeBlock } from "@/components/code-block";
import { JsonLd } from "@/components/json-ld";
import { LearnLayout } from "@/components/learn-layout";
import { chapterKind, chapters, getChapter } from "@/lib/docs";
import { isLanguage, languages } from "@/lib/i18n";
import { hhyVersion } from "@/lib/release";
import { createMetadata, localizedUrl, siteName } from "@/lib/seo";

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
        <p className="eyebrow">{chapterKind(chapter) === "guide"
          ? (lang === "zh" ? `指南 · 第 ${chapter.order} 章` : `Guide · Chapter ${chapter.order}`)
          : chapterKind(chapter) === "project"
            ? (lang === "zh" ? "实战项目 · 已通过自测" : "Project · Self-tested")
          : chapterKind(chapter) === "reference"
            ? (lang === "zh" ? "HHY 参考" : "HHY Reference")
          : chapterKind(chapter) === "extension"
            ? (lang === "zh" ? "HHY 扩展 · v1.1 已实现" : "HHY Extension · Implemented in v1.1")
            : (lang === "zh" ? "HHY 路线图" : "HHY Roadmap")}</p>
        <h1>{chapter.title[lang]}</h1>
        <p className="chapter-summary">{chapter.summary[lang]}</p>
        {chapter.sections[lang].map((section) => (
          <section key={section.title}>
            <h2>{section.title}</h2>
            {section.blocks.map((block, index) => {
              if (block.type === "p") return <p key={index}>{block.text}</p>;
              if (block.type === "note") return <aside className="doc-note" key={index}>{block.text}</aside>;
              if (block.type === "list") return <ul key={index}>{block.items.map((item) => <li key={item}>{item}</li>)}</ul>;
              if (block.type === "table") return (
                <div className="doc-table-wrap" key={index}>
                  <table className="doc-table">
                    <thead><tr>{block.columns.map((column) => <th key={column}>{column}</th>)}</tr></thead>
                    <tbody>{block.rows.map((row, rowIndex) => <tr key={rowIndex}>{row.map((cell, cellIndex) => <td data-label={block.columns[cellIndex]} key={cellIndex}>{cell}</td>)}</tr>)}</tbody>
                  </table>
                </div>
              );
              if (block.type === "link") return <a className="doc-link-card" href={block.href} target={block.href.startsWith("/") ? undefined : "_blank"} rel={block.href.startsWith("/") ? undefined : "noreferrer"} key={index}><strong>{block.label}</strong><span>{block.description}</span></a>;
              if (block.type === "image") return <figure className={`doc-image ${block.size}`} key={index}><a href={block.src} target="_blank" rel="noreferrer" aria-label={lang === "zh" ? "查看原图" : "View full-size image"}><Image src={block.src} alt={block.alt} width={block.width} height={block.height} sizes={block.size === "medium" ? "(max-width: 560px) calc(100vw - 32px), 480px" : "(max-width: 720px) calc(100vw - 32px), 680px"} /></a><figcaption>{block.caption}</figcaption></figure>;
              if (block.type === "evolution-roadmap") {
                const released: Array<{ version: string; date: string; title: string; detail: string; icon: ElementType }> = lang === "zh"
                  ? [
                    { version: "v1.0.0", date: "2026-08-25", title: "核心语义冻结", detail: "Pipe / Value / Stream / Error、94 个核心 callable、三平台发布验证", icon: Code },
                    { version: "v1.1.0", date: "2026-08-26", title: "本地进程扩展", detail: "install/list/remove、Protocol 1、database 0.2.0、三平台发布验证", icon: PuzzlePiece }
                  ]
                  : [
                    { version: "v1.0.0", date: "2026-08-25", title: "Core semantics frozen", detail: "Pipe / Value / Stream / Error, 94 core callables, and three-platform release evidence", icon: Code },
                    { version: "v1.1.0", date: "2026-08-26", title: "Local process extensions", detail: "install/list/remove, Protocol 1, database 0.2.0, and three-platform release evidence", icon: PuzzlePiece }
                  ];
                const releases: Array<{ version: string; window: string; title: string; items: string[]; icon: ElementType }> = lang === "zh"
                  ? [
                    { version: "v1.2", window: "2026 Q4–2027 Q1", title: "协议补全与 Office 验证", items: ["Stream / cancel / handle 生命周期", "官方 Office 扩展压力验证", "capability 与资源清理闭环"], icon: PuzzlePiece },
                    { version: "v1.3", window: "2027 Q2", title: "数据库资源模型", items: ["连接 handle 与连接池", "流式查询与背压", "数据库类型映射和事务稳定化"], icon: Database },
                    { version: "v1.4", window: "2027 Q3", title: "包分发与工具链", items: ["签名与发布者验证", "依赖解析和远程索引", "离线锁定与可复现安装"], icon: Handshake },
                    { version: "v1.5", window: "2027 Q4–2028 Q1", title: "Runtime 长期稳定化", items: ["trace / profile / debug hooks", "性能基线与兼容矩阵", "fuzz、故障注入与压力测试"], icon: ShieldCheck },
                    { version: "v2.0", window: "最早 2028 H2", title: "生态开放与 ABI 决策", items: ["以真实集成测量协议边界", "评估 embedding / FFI", "仅在必要时发布 Native ABI"], icon: RocketLaunch }
                  ]
                  : [
                    { version: "v1.2", window: "2026 Q4–2027 Q1", title: "Protocol and Office validation", items: ["Stream / cancel / handle lifecycle", "Official Office extension stress validation", "Capability and cleanup closure"], icon: PuzzlePiece },
                    { version: "v1.3", window: "2027 Q2", title: "Database resource model", items: ["Connection handles and pools", "Streaming queries with backpressure", "Type mapping and transaction hardening"], icon: Database },
                    { version: "v1.4", window: "2027 Q3", title: "Package distribution and tooling", items: ["Signature and publisher verification", "Dependency resolution and remote index", "Offline lock and reproducible install"], icon: Handshake },
                    { version: "v1.5", window: "2027 Q4–2028 Q1", title: "Long-term Runtime hardening", items: ["Trace / profile / debug hooks", "Performance baseline and compatibility matrix", "Fuzzing, fault injection, and stress tests"], icon: ShieldCheck },
                    { version: "v2.0", window: "2028 H2 at the earliest", title: "Ecosystem and ABI decision", items: ["Measure protocol limits with real integrations", "Evaluate embedding / FFI", "Publish a Native ABI only if necessary"], icon: RocketLaunch }
                  ];
                const principles = lang === "zh"
                  ? ["先冻结语义，再开放扩展", "先可用、可测，再做高性能", "扩展通过协议接入，不另造语义", "ABI 只在 Runtime 稳定后评估"]
                  : ["Freeze semantics before opening extensions", "Make it usable and measurable before fast", "Extend through protocol, not a second language model", "Evaluate ABI only after Runtime stability"];
                return (
                  <figure className="evolution-roadmap" key={index}>
                    <header><strong>{lang === "zh" ? "语言 / VM 演进路线图" : "Language / VM Evolution Roadmap"}</strong><span>{lang === "zh" ? "五个未来版本 · 建议窗口，不是发布日期承诺" : "Five future releases · recommended windows, not date commitments"}</span></header>
                    <div className="evolution-released">
                      <strong>{lang === "zh" ? "已发布基础" : "Released foundation"}</strong>
                      {released.map(({ version, date, title, detail, icon: Icon }) => <article key={version}><Icon size={42} weight="duotone" aria-hidden /><div><span><b>{version}</b><time>{date}</time><em>{lang === "zh" ? "已发布" : "Released"}</em></span><h3>{title}</h3><p>{detail}</p></div></article>)}
                    </div>
                    <div className="evolution-future-label"><span>{lang === "zh" ? "未来五个版本" : "Five future releases"}</span><small>{lang === "zh" ? "按验收门槛依次进入" : "Enter sequentially through acceptance gates"}</small></div>
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
                    <figcaption>{lang === "zh" ? "最佳实践顺序：语义与协议 → 资源模型 → 工具链 → Runtime 稳定化 → 生态与 ABI 决策" : "Best-practice order: semantics and protocol → resource model → tooling → Runtime hardening → ecosystem and ABI decision"}</figcaption>
                  </figure>
                );
              }
              if (block.type === "extension-flow") {
                const steps: Array<[string, string, string, ElementType]> = lang === "zh"
                  ? [
                    ["HHY 脚本", "导入数据库", "import database", Code],
                    ["Runtime", "验证并初始化", "校验清单与 SHA-256", ShieldCheck],
                    ["扩展进程", "启动隔离进程", "bin/hhy-database\n--protocol 1", RocketLaunch],
                    ["协议注册", "注册可调用能力", "handshake\nregister 4 个 callable", PuzzlePiece],
                    ["驱动适配", "连接数据库驱动", "libpq / MySQL client", Database],
                    ["调用执行", "返回结果或错误", "call_result\n或结构化 Error", TerminalWindow]
                  ]
                  : [
                    ["HHY script", "Import database", "import database", Code],
                    ["Runtime", "Validate and initialize", "Verify manifest and SHA-256", ShieldCheck],
                    ["Extension process", "Start isolated process", "bin/hhy-database\n--protocol 1", RocketLaunch],
                    ["Protocol registration", "Register callable capability", "handshake\nregister 4 callables", PuzzlePiece],
                    ["Driver adapter", "Connect database driver", "libpq / MySQL client", Database],
                    ["Call execution", "Return result or error", "call_result\nor structured Error", TerminalWindow]
                  ];
                const guarantees: Array<[string, string, ElementType]> = lang === "zh"
                  ? [["完整性验证", "安装与加载时校验 SHA-256", ShieldCheck], ["标准协议", "Protocol 1 统一互操作", Handshake], ["能力注册", "4 个 database callable", PuzzlePiece], ["统一响应", "结构化结果与错误", TerminalWindow]]
                  : [["Integrity", "Verify SHA-256 on install and load", ShieldCheck], ["Standard protocol", "Protocol 1 interoperability", Handshake], ["Capability registry", "Four database callables", PuzzlePiece], ["Unified response", "Structured results and errors", TerminalWindow]];
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
