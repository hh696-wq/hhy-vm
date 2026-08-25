import type { Metadata } from "next";
import { ArrowRight, BookOpenText } from "@phosphor-icons/react/dist/ssr";
import Link from "next/link";
import { notFound } from "next/navigation";
import { LearnLayout } from "@/components/learn-layout";
import { chapters } from "@/lib/docs";
import { isLanguage } from "@/lib/i18n";

export async function generateMetadata({ params }: { params: Promise<{ lang: string }> }): Promise<Metadata> {
  const { lang } = await params;
  return { title: lang === "zh" ? "学习 HHY" : "Learn HHY" };
}

export default async function LearnIndex({ params }: { params: Promise<{ lang: string }> }) {
  const { lang } = await params;
  if (!isLanguage(lang)) notFound();

  return (
    <LearnLayout language={lang}>
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
