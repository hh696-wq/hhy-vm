"use client";

import { CaretDown, ListBullets } from "@phosphor-icons/react";
import { useEffect, useState } from "react";
import type { Language } from "@/lib/i18n";

export function ChapterToc({ language, sections, mobile = false }: { language: Language; sections: string[]; mobile?: boolean }) {
  const [active, setActive] = useState(0);

  useEffect(() => {
    const sidebar = document.querySelector<HTMLElement>(".docs-sidebar");
    const currentChapter = sidebar?.querySelector<HTMLElement>("a.active");
    if (sidebar && currentChapter) {
      sidebar.scrollTop = Math.max(0, currentChapter.offsetTop - sidebar.clientHeight * 0.28);
    }

    let frame = 0;
    const updateActiveSection = () => {
      window.cancelAnimationFrame(frame);
      frame = window.requestAnimationFrame(() => {
        let current = 0;
        sections.forEach((_, index) => {
          const section = document.getElementById(`section-${index + 1}`);
          if (section && section.getBoundingClientRect().top <= 180) current = index;
        });
        setActive(current);
      });
    };

    updateActiveSection();
    window.addEventListener("scroll", updateActiveSection, { passive: true });
    return () => {
      window.cancelAnimationFrame(frame);
      window.removeEventListener("scroll", updateActiveSection);
    };
  }, [sections]);

  return (
    <details className={mobile ? "chapter-toc mobile-chapter-toc" : "chapter-toc"} open={!mobile}>
      <summary><ListBullets size={16} />{language === "zh" ? "本页目录" : "On this page"}<CaretDown className="chapter-toc-caret" size={15} /></summary>
      <nav aria-label={language === "zh" ? "本页目录" : "On this page"}>
        {sections.map((title, index) => (
          <a aria-current={active === index ? "location" : undefined} className={active === index ? "active" : undefined} href={`#section-${index + 1}`} key={title} onClick={() => setActive(index)}>
            <span>{String(index + 1).padStart(2, "0")}</span>{title}
          </a>
        ))}
      </nav>
    </details>
  );
}
