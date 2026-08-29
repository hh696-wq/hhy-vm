import { notFound } from "next/navigation";
import { SiteFooter } from "@/components/site-footer";
import { SiteHeader } from "@/components/site-header";
import { isLanguage, languages } from "@/lib/i18n";
import { getSearchDocuments } from "@/lib/search";

export function generateStaticParams() {
  return languages.map((lang) => ({ lang }));
}

export default async function LanguageLayout({ children, params }: { children: React.ReactNode; params: Promise<{ lang: string }> }) {
  const { lang } = await params;
  if (!isLanguage(lang)) notFound();

  return (
    <div className="site-frame" lang={lang === "zh" ? "zh-CN" : "en"}>
      <SiteHeader language={lang} searchDocuments={getSearchDocuments(lang)} />
      {children}
      <SiteFooter language={lang} />
    </div>
  );
}
