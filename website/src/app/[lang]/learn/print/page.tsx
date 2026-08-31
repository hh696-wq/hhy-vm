import type { Metadata } from "next";
import { notFound } from "next/navigation";
import { PrintManual } from "@/components/print-manual";
import { chapterKind, chapters } from "@/lib/docs";
import { isLanguage, languages } from "@/lib/i18n";
import { hhyVersionLabel } from "@/lib/release";

export function generateStaticParams() {
  return languages.map((lang) => ({ lang }));
}

export async function generateMetadata({ params }: { params: Promise<{ lang: string }> }): Promise<Metadata> {
  const { lang } = await params;
  if (!isLanguage(lang)) return {};
  return { title: lang === "zh" ? `HHY 语言手册 ${hhyVersionLabel} · 打印版` : `HHY Language Manual ${hhyVersionLabel} · Print edition`, robots: { index: false, follow: false } };
}

export default async function PrintManualPage({ params }: { params: Promise<{ lang: string }> }) {
  const { lang } = await params;
  if (!isLanguage(lang)) notFound();
  const groupOrder = { guide: 0, project: 1, reference: 2, extension: 3, roadmap: 4, tooling: 5, report: 6 };
  const orderedChapters = [...chapters].sort((a, b) => groupOrder[chapterKind(a)] - groupOrder[chapterKind(b)] || a.order - b.order);
  return <PrintManual language={lang} chapters={orderedChapters} />;
}
