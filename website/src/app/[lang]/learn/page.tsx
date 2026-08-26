import type { Metadata } from "next";
import { ArrowRight, BookOpenText } from "@phosphor-icons/react/dist/ssr";
import Link from "next/link";
import { notFound } from "next/navigation";
import { JsonLd } from "@/components/json-ld";
import { LearnLayout } from "@/components/learn-layout";
import { chapterKind, chapters } from "@/lib/docs";
import { isLanguage } from "@/lib/i18n";
import { createMetadata, localizedUrl } from "@/lib/seo";

export async function generateMetadata({ params }: { params: Promise<{ lang: string }> }): Promise<Metadata> {
  const { lang } = await params;
  if (!isLanguage(lang)) return {};
  return createMetadata({
    language: lang,
    path: "/learn",
    title: lang === "zh" ? "HHY 中文教程与语言手册" : "HHY Tutorial and Language Manual",
    description: lang === "zh"
      ? "HHY 1.0.0 中文教程与实战案例：学习 Flow、文件、JSON、进程、HTTP、并发，并用 HHY 完成日志、巡检、构建、报表和备份任务。"
      : "The HHY 1.0.0 tutorial and practical recipes for Flow, files, JSON, processes, HTTP, parallelism, logs, health checks, builds, reports, and backups.",
    keywords: lang === "zh" ? ["HHY中文教程", "HHY语言手册", "hhy run"] : ["HHY tutorial", "HHY language manual", "hhy run"]
  });
}

export default async function LearnIndex({ params }: { params: Promise<{ lang: string }> }) {
  const { lang } = await params;
  if (!isLanguage(lang)) notFound();
  const guideChapters = chapters.filter((chapter) => chapterKind(chapter) === "guide").sort((a, b) => a.order - b.order);
  const projectChapters = chapters.filter((chapter) => chapterKind(chapter) === "project").sort((a, b) => a.order - b.order);
  const referenceChapters = chapters.filter((chapter) => chapterKind(chapter) === "reference").sort((a, b) => a.order - b.order);
  const roadmapChapters = chapters.filter((chapter) => chapterKind(chapter) === "roadmap").sort((a, b) => a.order - b.order);

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
        <p className="eyebrow">HHY V1.0.0</p>
        <h1>{lang === "zh" ? "学习 HHY" : "Learn HHY"}</h1>
        <p>{lang === "zh" ? "从安装解释器开始，逐步掌握 Flow、系统标准库和可靠的自动化脚本，并通过日志、巡检、构建、报表和备份案例直接落地。这里的示例只描述 V1.0.0 已实现并验证的行为。" : "Start with the interpreter, master Flow and the system standard library, then apply HHY to logs, health checks, builds, reports, and backups. Every example documents behavior implemented and verified in V1.0.0."}</p>
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
      <p className="docs-index-description">{lang === "zh" ? "阅读一个由 HHY v1.0 完整实现、包含真实数据与端到端测试的应用。" : "Study a complete HHY v1.0 application with real fixtures and end-to-end tests."}</p>
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
      <h2 className="docs-index-heading reference-heading">{lang === "zh" ? "路线图" : "Roadmap"}</h2>
      <p className="docs-index-description">{lang === "zh" ? "查看尚未实现的版本计划、设计边界和明确标注的探索性 API。" : "Review unimplemented release plans, design boundaries, and explicitly labeled exploratory APIs."}</p>
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
    </LearnLayout>
  );
}
