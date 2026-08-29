"use client";

import { List, X } from "@phosphor-icons/react";
import Image from "next/image";
import Link from "next/link";
import { usePathname } from "next/navigation";
import { useEffect, useState } from "react";
import type { Language } from "@/lib/i18n";
import { otherLanguage, ui } from "@/lib/i18n";
import type { SearchDocument } from "@/lib/search";
import { DocsSearch } from "./docs-search";

export function SiteHeader({ language, searchDocuments }: { language: Language; searchDocuments: SearchDocument[] }) {
  const [open, setOpen] = useState(false);
  const pathname = usePathname();
  const copy = ui[language];
  const other = otherLanguage(language);
  const switchedPath = pathname.replace(`/${language}`, `/${other}`);
  const nav = [
    { label: copy.learn, href: `/${language}/learn` },
    { label: language === "zh" ? "示例" : "Examples", href: `/${language}/learn/practical-recipes` },
    { label: language === "zh" ? "扩展" : "Extensions", href: `/${language}/learn/extensions-roadmap` },
    { label: copy.spec, href: "https://github.com/hh696-wq/hhy-vm/blob/main/docs/HHY_V1.md", external: true },
    { label: copy.github, href: "https://github.com/hh696-wq/hhy-vm", external: true }
  ];

  function isActive(href: string) {
    if (href.startsWith("http")) return false;
    if (href.endsWith("/practical-recipes") || href.endsWith("/extensions-roadmap")) return pathname === href;
    if (href.endsWith("/learn") && (pathname.endsWith("/practical-recipes") || pathname.endsWith("/extensions-roadmap"))) return false;
    return pathname === href || pathname.startsWith(`${href}/`);
  }

  useEffect(() => {
    document.documentElement.lang = language === "zh" ? "zh-CN" : "en";
  }, [language]);

  return (
    <header className="site-header">
      <div className="header-inner">
        <Link className="brand" href={`/${language}`} aria-label="HHY Language home">
          <Image src="/hhy-logo.png" alt="HHY" width={72} height={72} priority />
          <span>HHY</span>
        </Link>

        <nav className={`main-nav${open ? " is-open" : ""}`} aria-label={language === "zh" ? "主导航" : "Main navigation"}>
          {nav.map((item) => (
            <Link className={isActive(item.href) ? "active" : undefined} aria-current={isActive(item.href) ? "page" : undefined} key={item.href} href={item.href} target={item.external ? "_blank" : undefined} onClick={() => setOpen(false)}>
              {item.label}
            </Link>
          ))}
          <Link className="language-link" href={switchedPath} onClick={() => setOpen(false)}>
            {other === "zh" ? "中文" : "EN"}
          </Link>
        </nav>

        <div className="header-actions">
          <DocsSearch language={language} documents={searchDocuments} />
          <Link className="language-link desktop-language" href={switchedPath}>{other === "zh" ? "中文" : "EN"}</Link>
          <button className="menu-button" type="button" onClick={() => setOpen((value) => !value)} aria-expanded={open} aria-label={language === "zh" ? "打开导航" : "Open navigation"}>
            {open ? <X size={25} /> : <List size={25} />}
          </button>
        </div>
      </div>
    </header>
  );
}
