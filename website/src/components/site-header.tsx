"use client";

import { DownloadSimple, GithubLogo, List, X } from "@phosphor-icons/react";
import Image from "next/image";
import Link from "next/link";
import { usePathname } from "next/navigation";
import { useEffect, useState } from "react";
import type { Language } from "@/lib/i18n";
import { otherLanguage, ui } from "@/lib/i18n";

export function SiteHeader({ language }: { language: Language }) {
  const [open, setOpen] = useState(false);
  const pathname = usePathname();
  const copy = ui[language];
  const other = otherLanguage(language);
  const switchedPath = pathname.replace(`/${language}`, `/${other}`);
  const nav = [
    { label: copy.learn, href: `/${language}/learn` },
    { label: copy.spec, href: "https://github.com/hh696-wq/hhy-vm/blob/main/docs/HHY_V1.md", external: true },
    { label: copy.github, href: "https://github.com/hh696-wq/hhy-vm", external: true }
  ];

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
            <Link key={item.href} href={item.href} target={item.external ? "_blank" : undefined} onClick={() => setOpen(false)}>
              {item.label}
              {item.external && item.label === copy.github ? <GithubLogo size={16} weight="fill" /> : null}
            </Link>
          ))}
          <Link className="language-link" href={switchedPath} onClick={() => setOpen(false)}>
            {other === "zh" ? "中文" : "EN"}
          </Link>
          <Link className="download-button mobile-download" href="https://github.com/hh696-wq/hhy-vm/releases/latest" target="_blank" onClick={() => setOpen(false)}>
            <DownloadSimple size={18} /> {copy.download}
          </Link>
        </nav>

        <div className="header-actions">
          <Link className="language-link desktop-language" href={switchedPath}>{other === "zh" ? "中文" : "EN"}</Link>
          <Link className="download-button" href="https://github.com/hh696-wq/hhy-vm/releases/latest" target="_blank">
            <DownloadSimple size={19} /> {copy.download}
          </Link>
          <button className="menu-button" type="button" onClick={() => setOpen((value) => !value)} aria-expanded={open} aria-label={language === "zh" ? "打开导航" : "Open navigation"}>
            {open ? <X size={25} /> : <List size={25} />}
          </button>
        </div>
      </div>
    </header>
  );
}
