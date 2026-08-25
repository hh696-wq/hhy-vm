import { EnvelopeSimple, GithubLogo, GlobeSimple } from "@phosphor-icons/react/dist/ssr";
import Image from "next/image";
import Link from "next/link";
import type { Language } from "@/lib/i18n";
import { ui } from "@/lib/i18n";

export function SiteFooter({ language }: { language: Language }) {
  const copy = ui[language];
  const groups = language === "zh" ? [
    { title: "学习", links: [["快速开始", "quick-start"], ["完整手册", ""], ["CLI 参考", "cli-reference"]] }
  ] : [
    { title: "Learn", links: [["Quick Start", "quick-start"], ["Complete Manual", ""], ["CLI Reference", "cli-reference"]] }
  ];

  return (
    <footer className="site-footer">
      <div className="footer-inner">
        <div className="footer-brand">
          <div className="footer-logo"><Image src="/hhy-logo.png" alt="HHY" width={64} height={64} /><strong>HHY Language</strong></div>
          <p>{copy.footerLine}</p>
          <small>© 2026 HHY Language contributors</small>
        </div>
        {groups.map((group) => (
          <div className="footer-group" key={group.title}>
            <strong>{group.title}</strong>
            {group.links.map(([label, slug]) => <Link key={label} href={`/${language}/learn${slug ? `/${slug}` : ""}`}>{label}</Link>)}
          </div>
        ))}
        <div className="footer-group">
          <strong>{language === "zh" ? "项目" : "Project"}</strong>
          <Link href="https://github.com/hh696-wq/hhy-vm" target="_blank"><GithubLogo size={18} weight="fill" /> GitHub</Link>
          <Link href="https://github.com/hh696-wq/hhy-vm/blob/main/docs/HHY_V1.md" target="_blank">{copy.spec}</Link>
          <Link href="https://github.com/hh696-wq/hhy-vm/blob/main/LICENSE" target="_blank">Apache 2.0</Link>
        </div>
        <div className="footer-group">
          <strong>{language === "zh" ? "联系" : "Contact"}</strong>
          <Link href="https://hhylang.dev" target="_blank"><GlobeSimple size={18} /> hhylang.dev</Link>
          <Link href="mailto:huiyang.hou@qq.com"><EnvelopeSimple size={18} /> huiyang.hou@qq.com</Link>
        </div>
      </div>
    </footer>
  );
}
