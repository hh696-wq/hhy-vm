import type { Metadata } from "next";
import { notFound } from "next/navigation";
import { CodeBlock } from "@/components/code-block";
import { JsonLd } from "@/components/json-ld";
import { LearnLayout } from "@/components/learn-layout";
import { chapterKind, chapters, getChapter } from "@/lib/docs";
import { isLanguage, languages } from "@/lib/i18n";
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
        version: "1.0.0",
        about: ["HHY Language", "system scripting", "Flow pipelines"]
      }} />
      <article className="chapter-article">
        <p className="eyebrow">{chapterKind(chapter) === "guide"
          ? (lang === "zh" ? `指南 · 第 ${chapter.order} 章` : `Guide · Chapter ${chapter.order}`)
          : chapterKind(chapter) === "reference"
            ? (lang === "zh" ? "HHY 参考" : "HHY Reference")
            : (lang === "zh" ? "路线图 · 尚未实现" : "Roadmap · Not implemented")}</p>
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
                    <tbody>{block.rows.map((row, rowIndex) => <tr key={rowIndex}>{row.map((cell, cellIndex) => <td key={cellIndex}>{cell}</td>)}</tr>)}</tbody>
                  </table>
                </div>
              );
              if (block.type === "link") return <a className="doc-link-card" href={block.href} target={block.href.startsWith("/") ? undefined : "_blank"} rel={block.href.startsWith("/") ? undefined : "noreferrer"} key={index}><strong>{block.label}</strong><span>{block.description}</span></a>;
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
              return <CodeBlock code={block.code} language={block.language} filename={block.language === "hhy" ? "example.hhy" : undefined} locale={lang} compact key={index} />;
            })}
          </section>
        ))}
      </article>
    </LearnLayout>
  );
}
