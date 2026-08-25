import type { Metadata } from "next";
import { notFound } from "next/navigation";
import { HomePage } from "@/components/home-page";
import { isLanguage } from "@/lib/i18n";

export async function generateMetadata({ params }: { params: Promise<{ lang: string }> }): Promise<Metadata> {
  const { lang } = await params;
  return {
    title: lang === "zh" ? "管道即语言" : "Pipe Everything",
    description: lang === "zh" ? "HHY 是一门面向系统自动化、以数据流为核心的脚本语言。" : "HHY is a flow-first scripting language for system automation."
  };
}

export default async function LanguageHome({ params }: { params: Promise<{ lang: string }> }) {
  const { lang } = await params;
  if (!isLanguage(lang)) notFound();
  return <HomePage language={lang} />;
}
