import Image from "next/image";
import type { DocBlock, Chapter } from "@/lib/docs";
import { chapterKind } from "@/lib/docs";
import type { Language } from "@/lib/i18n";
import { hhyVersionLabel } from "@/lib/release";
import { PrintButton } from "@/components/print-button";

const groupLabel = {
  zh: { guide: "指南", project: "实战项目", reference: "参考", extension: "扩展", roadmap: "路线图" },
  en: { guide: "Guide", project: "Projects", reference: "Reference", extension: "Extensions", roadmap: "Roadmap" }
} as const;

function PrintBlock({ block, language }: { block: DocBlock; language: Language }) {
  if (block.type === "p") return <p>{block.text}</p>;
  if (block.type === "note") return <aside className="print-note">{block.text}</aside>;
  if (block.type === "list") return <ul>{block.items.map((item) => <li key={item}>{item}</li>)}</ul>;
  if (block.type === "code") return <figure className="print-code"><figcaption>{block.filename ?? block.language}</figcaption><pre><code>{block.code}</code></pre></figure>;
  if (block.type === "terminal") return <figure className="print-terminal"><figcaption>$ {block.command}</figcaption><pre>{block.output}</pre></figure>;
  if (block.type === "terminal-card") return <figure className="print-terminal"><figcaption>{block.title} · $ {block.command}</figcaption><pre>{block.output}</pre><p>{block.caption}</p></figure>;
  if (block.type === "table") return <div className="print-table-wrap"><table><thead><tr>{block.columns.map((column) => <th key={column}>{column}</th>)}</tr></thead><tbody>{block.rows.map((row, rowIndex) => <tr key={rowIndex}>{row.map((cell, cellIndex) => <td key={cellIndex}>{cell}</td>)}</tr>)}</tbody></table></div>;
  if (block.type === "link") return <p className="print-link"><strong>{block.label}</strong><br /><span>{block.description}</span><br /><a href={block.href}>{block.href}</a></p>;
  if (block.type === "image") return <figure className="print-image"><Image src={block.src} alt={block.alt} width={block.width} height={block.height} /><figcaption>{block.caption}</figcaption></figure>;
  if (block.type === "api") return <div className="print-api">{block.entries.map((entry) => <article key={entry.name}><h3>{entry.name}</h3><pre><code>{entry.signature}</code></pre><p>{entry.description}</p></article>)}</div>;

  const special = language === "zh" ? {
    "extension-flow": ["扩展加载流程", "HHY 脚本 → Runtime 校验 → 隔离扩展进程 → 协议注册 → 调用执行 → 结构化结果或错误。"],
    "evolution-roadmap": ["语言与 VM 演进路线图", "核心语义冻结后，依次推进性能加固、官方扩展工具链，以及由真实生态证据驱动的 ABI 决策。"],
    "runtime-performance-roadmap": ["AST 解释器性能演进", "AST 预解析 → Slot 绑定 → Lightweight CallFrame → Identifier Cache → 逃逸安全的 Frame 复用 → Profiling 决策。"]
  } : {
    "extension-flow": ["Extension loading flow", "HHY script → Runtime validation → isolated extension process → protocol registration → call execution → structured result or error."],
    "evolution-roadmap": ["Language and VM evolution", "After core semantics freeze: performance hardening, official extension tooling, and an ABI decision driven by real ecosystem evidence."],
    "runtime-performance-roadmap": ["AST interpreter performance evolution", "AST pre-resolution → slot binding → lightweight CallFrame → identifier cache → escape-safe frame reuse → profiling decision."]
  };
  const [title, detail] = special[block.type];
  return <aside className="print-diagram-summary"><strong>{title}</strong><p>{detail}</p></aside>;
}

export function PrintManual({ language, chapters }: { language: Language; chapters: Chapter[] }) {
  const title = language === "zh" ? "HHY 语言手册" : "HHY Language Manual";
  return (
    <main className="print-manual">
      <div className="print-actions"><PrintButton language={language} /></div>
      <section className="print-cover">
        <Image src="/hhy-logo.png" alt="HHY" width={180} height={180} priority />
        <p>HHY LANGUAGE</p>
        <h1>{title}</h1>
        <strong>{hhyVersionLabel}</strong>
        <span>{language === "zh" ? "Flow-first 系统脚本语言 · 中文完整手册" : "Flow-first system scripting language · Complete manual"}</span>
        <small>hhylang.dev</small>
      </section>

      <section className="print-toc">
        <p className="print-kicker">CONTENTS</p>
        <h2>{language === "zh" ? "目录" : "Table of contents"}</h2>
        <ol>{chapters.map((chapter, index) => <li key={chapter.slug}><a href={`#chapter-${chapter.slug}`}><span>{String(index + 1).padStart(2, "0")}</span><strong>{chapter.title[language]}</strong><em>{groupLabel[language][chapterKind(chapter)]}</em></a></li>)}</ol>
      </section>

      {chapters.map((chapter, chapterIndex) => (
        <article className="print-chapter" id={`chapter-${chapter.slug}`} key={chapter.slug}>
          <header>
            <p className="print-kicker">{groupLabel[language][chapterKind(chapter)]} · {String(chapterIndex + 1).padStart(2, "0")}</p>
            <h1>{chapter.title[language]}</h1>
            <p>{chapter.summary[language]}</p>
          </header>
          {chapter.sections[language].map((section, sectionIndex) => <section key={section.title}>
            <h2><span>{chapterIndex + 1}.{sectionIndex + 1}</span>{section.title}</h2>
            {section.blocks.map((block, blockIndex) => <PrintBlock block={block} language={language} key={blockIndex} />)}
          </section>)}
        </article>
      ))}
    </main>
  );
}
