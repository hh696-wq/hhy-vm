import { ArrowLeft, ArrowRight, ListBullets } from "@phosphor-icons/react/dist/ssr";
import Link from "next/link";
import type { ReactNode } from "react";
import { chapters } from "@/lib/docs";
import type { Chapter } from "@/lib/docs";
import type { Language } from "@/lib/i18n";

export function LearnLayout({ language, chapter, children }: { language: Language; chapter?: Chapter; children: ReactNode }) {
  const currentIndex = chapter ? chapters.findIndex((item) => item.slug === chapter.slug) : -1;
  const previous = currentIndex > 0 ? chapters[currentIndex - 1] : undefined;
  const next = currentIndex >= 0 && currentIndex < chapters.length - 1 ? chapters[currentIndex + 1] : undefined;

  return (
    <main className="docs-shell">
      <aside className="docs-sidebar">
        <div className="docs-sidebar-title"><ListBullets size={20} />{language === "zh" ? "HHY 手册" : "HHY Manual"}</div>
        <nav aria-label={language === "zh" ? "手册章节" : "Manual chapters"}>
          {chapters.map((item) => (
            <Link className={chapter?.slug === item.slug ? "active" : ""} href={`/${language}/learn/${item.slug}`} key={item.slug}>
              <span>{String(item.order).padStart(2, "0")}</span>{item.title[language]}
            </Link>
          ))}
        </nav>
        <Link className="spec-link" href="https://github.com/hh696-wq/hhy-vm/blob/main/docs/HHY_V1.md" target="_blank">{language === "zh" ? "V1.0 完整规范 ↗" : "Full V1.0 spec ↗"}</Link>
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
