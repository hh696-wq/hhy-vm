import { ArrowLeft, ArrowRight, CaretDown, FilePdf, ListBullets } from "@phosphor-icons/react/dist/ssr";
import Link from "next/link";
import { Fragment } from "react";
import type { ReactNode } from "react";
import { ChapterToc } from "@/components/chapter-toc";
import { chapterKind, chapters } from "@/lib/docs";
import type { Chapter } from "@/lib/docs";
import type { Language } from "@/lib/i18n";
import { hhyVersionLabel } from "@/lib/release";

export function LearnLayout({ language, chapter, children }: { language: Language; chapter?: Chapter; children: ReactNode }) {
  const guideChapters = chapters.filter((item) => chapterKind(item) === "guide").sort((a, b) => a.order - b.order);
  const projectChapters = chapters.filter((item) => chapterKind(item) === "project").sort((a, b) => a.order - b.order);
  const referenceChapters = chapters.filter((item) => chapterKind(item) === "reference").sort((a, b) => a.order - b.order);
  const extensionChapters = chapters.filter((item) => chapterKind(item) === "extension").sort((a, b) => a.order - b.order);
  const roadmapChapters = chapters.filter((item) => chapterKind(item) === "roadmap").sort((a, b) => a.order - b.order);
  const manualChapters = [...guideChapters, ...projectChapters, ...referenceChapters, ...extensionChapters, ...roadmapChapters];
  const currentIndex = chapter ? manualChapters.findIndex((item) => item.slug === chapter.slug) : -1;
  const previous = currentIndex > 0 ? manualChapters[currentIndex - 1] : undefined;
  const next = currentIndex >= 0 && currentIndex < manualChapters.length - 1 ? manualChapters[currentIndex + 1] : undefined;
  const currentSections = chapter?.sections[language].map((section) => section.title) ?? [];
  const renderChapterLink = (item: Chapter) => {
    const active = chapter?.slug === item.slug;
    return (
      <Fragment key={item.slug}>
        <Link aria-current={active ? "page" : undefined} className={active ? "active" : ""} href={`/${language}/learn/${item.slug}`}><span>{String(item.order).padStart(2, "0")}</span>{item.title[language]}</Link>
        {active && currentSections.length ? <ChapterToc language={language} sections={currentSections} /> : null}
      </Fragment>
    );
  };
  const renderMobileChapterLink = (item: Chapter) => {
    const active = chapter?.slug === item.slug;
    return <Link aria-current={active ? "page" : undefined} className={active ? "active" : ""} href={`/${language}/learn/${item.slug}`} key={item.slug}><span>{String(item.order).padStart(2, "0")}</span>{item.title[language]}</Link>;
  };

  return (
    <main className="docs-shell">
      <aside className="docs-sidebar">
        <div className="docs-sidebar-title"><ListBullets size={20} />{language === "zh" ? "HHY 手册" : "HHY Manual"}</div>
        <nav aria-label={language === "zh" ? "手册章节" : "Manual chapters"}>
          <small className="docs-nav-group">{language === "zh" ? "指南" : "Guide"}</small>
          {guideChapters.map(renderChapterLink)}
          <small className="docs-nav-group">{language === "zh" ? "实战项目" : "Project"}</small>
          {projectChapters.map(renderChapterLink)}
          <small className="docs-nav-group">{language === "zh" ? "参考" : "Reference"}</small>
          {referenceChapters.map(renderChapterLink)}
          <small className="docs-nav-group">{language === "zh" ? "扩展" : "Extensions"}</small>
          {extensionChapters.map(renderChapterLink)}
          <small className="docs-nav-group">{language === "zh" ? "路线图" : "Roadmap"}</small>
          {roadmapChapters.map(renderChapterLink)}
        </nav>
        <Link className="print-manual-link" href={`/${language}/learn/print`}><FilePdf size={17} />{language === "zh" ? "打印版手册 / PDF" : "Print edition / PDF"}</Link>
        <Link className="spec-link" href="https://github.com/hh696-wq/hhy-vm/blob/main/docs/HHY_V1.md" target="_blank">{language === "zh" ? `${hhyVersionLabel} 完整规范 ↗` : `Full ${hhyVersionLabel} spec ↗`}</Link>
      </aside>
      <div className="docs-content">
        <details className="mobile-manual-toc">
          <summary aria-label={language === "zh" ? "打开全部章节" : "Open all chapters"}><ListBullets size={20} /><strong>{language === "zh" ? "全部章节" : "All chapters"}</strong><span>{manualChapters.length}</span><CaretDown size={16} /></summary>
          <nav aria-label={language === "zh" ? "完整手册目录" : "Complete manual contents"}>
            <small className="docs-nav-group">{language === "zh" ? "指南" : "Guide"}</small>
            {guideChapters.map(renderMobileChapterLink)}
            <small className="docs-nav-group">{language === "zh" ? "实战项目" : "Project"}</small>
            {projectChapters.map(renderMobileChapterLink)}
            <small className="docs-nav-group">{language === "zh" ? "参考" : "Reference"}</small>
            {referenceChapters.map(renderMobileChapterLink)}
            <small className="docs-nav-group">{language === "zh" ? "扩展" : "Extensions"}</small>
            {extensionChapters.map(renderMobileChapterLink)}
            <small className="docs-nav-group">{language === "zh" ? "路线图" : "Roadmap"}</small>
            {roadmapChapters.map(renderMobileChapterLink)}
          </nav>
        </details>
        {chapter && currentSections.length ? <ChapterToc language={language} mobile sections={currentSections} /> : null}
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
