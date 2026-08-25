import type { Metadata } from "next";
import { ArrowRight, BookOpenText } from "@phosphor-icons/react/dist/ssr";
import Link from "next/link";
import { notFound } from "next/navigation";
import { JsonLd } from "@/components/json-ld";
import { LearnLayout } from "@/components/learn-layout";
import { chapters } from "@/lib/docs";
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
      ? "HHY 1.0.0 中文教程：安装并运行 .hhy 脚本，学习变量、函数、Flow、文件、JSON、进程、HTTP、并发、模块和错误处理。"
      : "The HHY 1.0.0 tutorial: install and run .hhy scripts, then learn variables, functions, Flow, files, JSON, processes, HTTP, parallelism, modules, and errors.",
    keywords: lang === "zh" ? ["HHY中文教程", "HHY语言手册", "hhy run"] : ["HHY tutorial", "HHY language manual", "hhy run"]
  });
}

export default async function LearnIndex({ params }: { params: Promise<{ lang: string }> }) {
  const { lang } = await params;
  if (!isLanguage(lang)) notFound();

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
        <p>{lang === "zh" ? "从安装解释器开始，逐步掌握 Flow、系统标准库和可靠的自动化脚本。这里的示例只描述 V1.0.0 已实现并验证的行为。" : "Start with the interpreter, then master Flow, the system standard library, and reliable automation scripts. Every example documents behavior implemented and verified in V1.0.0."}</p>
      </div>
      <div className="chapter-grid">
        {chapters.map((chapter) => (
          <Link href={`/${lang}/learn/${chapter.slug}`} className="chapter-card" key={chapter.slug}>
            <span>{String(chapter.order).padStart(2, "0")}</span>
            <h2>{chapter.title[lang]}</h2>
            <p>{chapter.summary[lang]}</p>
            <strong>{lang === "zh" ? "阅读章节" : "Read chapter"}<ArrowRight size={17} /></strong>
          </Link>
        ))}
      </div>
    </LearnLayout>
  );
}
