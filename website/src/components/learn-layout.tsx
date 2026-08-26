import { ArrowLeft, ArrowRight, ListBullets } from "@phosphor-icons/react/dist/ssr";
import Link from "next/link";
import type { ReactNode } from "react";
import { chapterKind, chapters } from "@/lib/docs";
import type { Chapter } from "@/lib/docs";
import type { Language } from "@/lib/i18n";
import { hhyVersionLabel } from "@/lib/release";

export function LearnLayout({ language, chapter, children }: { language: Language; chapter?: Chapter; children: ReactNode }) {
  const guideChapters = chapters.filter((item) => chapterKind(item) === "guide").sort((a, b) => a.order - b.order);
  const projectChapters = chapters.filter((item) => chapterKind(item) === "project").sort((a, b) => a.order - b.order);
  const referenceChapters = chapters.filter((item) => chapterKind(item) === "reference").sort((a, b) => a.order - b.order);
  const roadmapChapters = chapters.filter((item) => chapterKind(item) === "roadmap").sort((a, b) => a.order - b.order);
  const manualChapters = [...guideChapters, ...projectChapters, ...referenceChapters, ...roadmapChapters];
  const currentIndex = chapter ? manualChapters.findIndex((item) => item.slug === chapter.slug) : -1;
  const previous = currentIndex > 0 ? manualChapters[currentIndex - 1] : undefined;
  const next = currentIndex >= 0 && currentIndex < manualChapters.length - 1 ? manualChapters[currentIndex + 1] : undefined;

  return (
    <main className="docs-shell">
      <aside className="docs-sidebar">
        <div className="docs-sidebar-title"><ListBullets size={20} />{language === "zh" ? "HHY 手册" : "HHY Manual"}</div>
        <nav aria-label={language === "zh" ? "手册章节" : "Manual chapters"}>
          <small className="docs-nav-group">{language === "zh" ? "指南" : "Guide"}</small>
          {guideChapters.map((item) => <Link className={chapter?.slug === item.slug ? "active" : ""} href={`/${language}/learn/${item.slug}`} key={item.slug}><span>{String(item.order).padStart(2, "0")}</span>{item.title[language]}</Link>)}
          <small className="docs-nav-group">{language === "zh" ? "实战项目" : "Project"}</small>
          {projectChapters.map((item) => <Link className={chapter?.slug === item.slug ? "active" : ""} href={`/${language}/learn/${item.slug}`} key={item.slug}><span>{String(item.order).padStart(2, "0")}</span>{item.title[language]}</Link>)}
          <small className="docs-nav-group">{language === "zh" ? "参考" : "Reference"}</small>
          {referenceChapters.map((item) => <Link className={chapter?.slug === item.slug ? "active" : ""} href={`/${language}/learn/${item.slug}`} key={item.slug}><span>{String(item.order).padStart(2, "0")}</span>{item.title[language]}</Link>)}
          <small className="docs-nav-group">{language === "zh" ? "路线图" : "Roadmap"}</small>
          {roadmapChapters.map((item) => <Link className={chapter?.slug === item.slug ? "active" : ""} href={`/${language}/learn/${item.slug}`} key={item.slug}><span>{String(item.order).padStart(2, "0")}</span>{item.title[language]}</Link>)}
        </nav>
        <Link className="spec-link" href="https://github.com/hh696-wq/hhy-vm/blob/main/docs/HHY_V1.md" target="_blank">{language === "zh" ? `${hhyVersionLabel} 完整规范 ↗` : `Full ${hhyVersionLabel} spec ↗`}</Link>
      </aside>
      <div className="docs-content">
        {children}
        {chapter ? (
          <nav className="chapter-pagination" aria-label={language === "zh" ? "章节翻页" : "Chapter pagination"}>
            {previous ? <Link href={`/${language}/learn/${previous.slug}`}><ArrowLeft size={18} /><span><small>{language === "zh" ? "上一章" : "Previous"}</small>{previous.title[language]}</span></Link> : <span />}
            {next ? <Link href={`/${language}/learn/${next.slug}`}><span><small>{language === "zh" ? "下一章" : "Next"}</small>{next.title[language]}</span><ArrowRight size={18} /></Link> : <span />}
          </nav>
        ) : null}
      </div>
    </main>
  );
}
