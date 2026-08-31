import type { Metadata } from "next";
import { ArrowRight, BookOpenText } from "@phosphor-icons/react/dist/ssr";
import Link from "next/link";
import { notFound } from "next/navigation";
import { JsonLd } from "@/components/json-ld";
import { LearnLayout } from "@/components/learn-layout";
import { chapterKind, chapters } from "@/lib/docs";
import { isLanguage } from "@/lib/i18n";
import { hhyVersion, hhyVersionLabel, hhyVersionTag } from "@/lib/release";
import { createMetadata, localizedUrl } from "@/lib/seo";

export async function generateMetadata({ params }: { params: Promise<{ lang: string }> }): Promise<Metadata> {
  const { lang } = await params;
  if (!isLanguage(lang)) return {};
  return createMetadata({
    language: lang,
    path: "/learn",
    title: lang === "zh" ? "HHY 中文教程与语言手册" : "HHY Tutorial and Language Manual",
    description: lang === "zh"
      ? `HHY ${hhyVersion} 中文教程与实战案例：学习 Flow、文件、JSON、进程、HTTP、并发、本地进程扩展与数据库扩展。`
      : `The HHY ${hhyVersion} tutorial and practical recipes for Flow, files, JSON, processes, HTTP, parallelism, local process extensions, and database integration.`,
    keywords: lang === "zh" ? ["HHY中文教程", "HHY语言手册", "hhy run"] : ["HHY tutorial", "HHY language manual", "hhy run"]
  });
}

export default async function LearnIndex({ params }: { params: Promise<{ lang: string }> }) {
  const { lang } = await params;
  if (!isLanguage(lang)) notFound();
  const guideChapters = chapters.filter((chapter) => chapterKind(chapter) === "guide").sort((a, b) => a.order - b.order);
  const projectChapters = chapters.filter((chapter) => chapterKind(chapter) === "project").sort((a, b) => a.order - b.order);
  const referenceChapters = chapters.filter((chapter) => chapterKind(chapter) === "reference").sort((a, b) => a.order - b.order);
  const extensionChapters = chapters.filter((chapter) => chapterKind(chapter) === "extension").sort((a, b) => a.order - b.order);
  const roadmapChapters = chapters.filter((chapter) => chapterKind(chapter) === "roadmap").sort((a, b) => a.order - b.order);
  const toolingChapters = chapters.filter((chapter) => chapterKind(chapter) === "tooling").sort((a, b) => a.order - b.order);

  return (
    <LearnLayout language={lang}>
      <JsonLd data={{
        "@context": "https://schema.org",
        "@type": "CollectionPage",
        name: lang === "zh" ? "HHY 中文教程与语言手册" : "HHY Tutorial and Language Manual",
        url: localizedUrl(lang, "/learn"),
        inLanguage: lang === "zh" ? "zh-CN" : "en",
        hasPart: chapters.map((chapter) => ({
          "@type": "TechArticle",
          name: chapter.title[lang],
          url: localizedUrl(lang, `/learn/${chapter.slug}`)
        }))
      }} />
      <div className="docs-hero">
        <span className="round-icon"><BookOpenText size={30} weight="duotone" /></span>
        <p className="eyebrow">HHY {hhyVersionLabel}</p>
        <h1>{lang === "zh" ? "学习 HHY" : "Learn HHY"}</h1>
        <p>{lang === "zh" ? `从安装解释器开始，逐步掌握 Flow、系统标准库和可靠的自动化脚本，再学习 ${hhyVersionLabel} 的本地进程扩展与数据库集成。文档会明确区分已实现能力和后续计划。` : `Start with the interpreter, master Flow and the system standard library, then learn the local process-extension and database integration delivered in ${hhyVersionLabel}. The manual clearly separates implemented behavior from future plans.`}</p>
      </div>
      <h2 className="docs-index-heading">{lang === "zh" ? "指南" : "Guide"}</h2>
      <p className="docs-index-description">{lang === "zh" ? "按顺序学习 HHY，并通过完整示例完成常见系统自动化任务。" : "Learn HHY in order and complete common system-automation tasks with working examples."}</p>
      <div className="chapter-grid">
        {guideChapters.map((chapter) => (
          <Link href={`/${lang}/learn/${chapter.slug}`} className="chapter-card" key={chapter.slug}>
            <span>{String(chapter.order).padStart(2, "0")}</span>
            <h2>{chapter.title[lang]}</h2>
            <p>{chapter.summary[lang]}</p>
            <strong>{lang === "zh" ? "阅读章节" : "Read chapter"}<ArrowRight size={17} /></strong>
          </Link>
        ))}
      </div>
      <h2 className="docs-index-heading reference-heading">{lang === "zh" ? "实战项目" : "Project"}</h2>
      <p className="docs-index-description">{lang === "zh" ? `阅读由 HHY ${hhyVersionTag} 验证、包含真实数据与端到端测试的完整应用。` : `Study complete applications verified with HHY ${hhyVersionTag}, real fixtures, and end-to-end tests.`}</p>
      <div className="chapter-grid">
        {projectChapters.map((chapter) => (
          <Link href={`/${lang}/learn/${chapter.slug}`} className="chapter-card project-card" key={chapter.slug}>
            <span>{String(chapter.order).padStart(2, "0")}</span>
            <h2>{chapter.title[lang]}</h2>
            <p>{chapter.summary[lang]}</p>
            <strong>{lang === "zh" ? "查看完整项目" : "Explore project"}<ArrowRight size={17} /></strong>
          </Link>
        ))}
      </div>
      <h2 className="docs-index-heading reference-heading">{lang === "zh" ? "参考" : "Reference"}</h2>
      <p className="docs-index-description">{lang === "zh" ? "精确查阅语法、标准库函数和命令行行为。" : "Look up exact syntax, standard-library symbols, and command behavior."}</p>
      <div className="chapter-grid">
        {referenceChapters.map((chapter) => (
          <Link href={`/${lang}/learn/${chapter.slug}`} className="chapter-card" key={chapter.slug}>
            <span>{String(chapter.order).padStart(2, "0")}</span>
            <h2>{chapter.title[lang]}</h2>
            <p>{chapter.summary[lang]}</p>
            <strong>{lang === "zh" ? "查阅参考" : "Open reference"}<ArrowRight size={17} /></strong>
          </Link>
        ))}
      </div>
      <h2 className="docs-index-heading reference-heading">{lang === "zh" ? "扩展" : "Extensions"}</h2>
      <p className="docs-index-description">{lang === "zh" ? "学习 HHY v1.1.6 的进程扩展系统、可恢复 Spider，并直接使用官方数据库与 HTML 扩展。" : "Learn the HHY v1.1.6 process-extension system, resumable spider, and official database and HTML extensions."}</p>
      <div className="chapter-grid">
        {extensionChapters.map((chapter) => (
          <Link href={`/${lang}/learn/${chapter.slug}`} className="chapter-card" key={chapter.slug}>
            <span>{String(chapter.order).padStart(2, "0")}</span>
            <h2>{chapter.title[lang]}</h2>
            <p>{chapter.summary[lang]}</p>
            <strong>{lang === "zh" ? "阅读扩展文档" : "Read extension guide"}<ArrowRight size={17} /></strong>
          </Link>
        ))}
      </div>
      <h2 className="docs-index-heading reference-heading">{lang === "zh" ? "路线图" : "Roadmap"}</h2>
      <p className="docs-index-description">{lang === "zh" ? "了解 v1.1 已实现的扩展能力、设计边界和明确标注的后续计划。" : "Review implemented v1.1 extension capabilities, design boundaries, and explicitly labeled future work."}</p>
      <div className="chapter-grid">
        {roadmapChapters.map((chapter) => (
          <Link href={`/${lang}/learn/${chapter.slug}`} className="chapter-card" key={chapter.slug}>
            <span>{String(chapter.order).padStart(2, "0")}</span>
            <h2>{chapter.title[lang]}</h2>
            <p>{chapter.summary[lang]}</p>
            <strong>{lang === "zh" ? "查看路线图" : "Read roadmap"}<ArrowRight size={17} /></strong>
          </Link>
        ))}
      </div>
      <h2 className="docs-index-heading reference-heading">{lang === "zh" ? "工具" : "Tooling"}</h2>
      <p className="docs-index-description">{lang === "zh" ? "安装与 HHY Lexer 同步生成的编辑器语言支持。" : "Install editor language support generated in sync with the HHY Lexer."}</p>
      <div className="chapter-grid">
        {toolingChapters.map((chapter) => (
          <Link href={`/${lang}/learn/${chapter.slug}`} className="chapter-card" key={chapter.slug}>
            <span>{String(chapter.order).padStart(2, "0")}</span>
            <h2>{chapter.title[lang]}</h2>
            <p>{chapter.summary[lang]}</p>
            <strong>{lang === "zh" ? "安装语言支持" : "Install language support"}<ArrowRight size={17} /></strong>
          </Link>
        ))}
      </div>
    </LearnLayout>
  );
}
