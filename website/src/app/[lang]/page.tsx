import type { Metadata } from "next";
import { notFound } from "next/navigation";
import { JsonLd } from "@/components/json-ld";
import { HomePage } from "@/components/home-page";
import { isLanguage } from "@/lib/i18n";
import { hhyVersion } from "@/lib/release";
import { createMetadata, localizedUrl, releaseUrl, siteName, siteOrigin } from "@/lib/seo";

export async function generateMetadata({ params }: { params: Promise<{ lang: string }> }): Promise<Metadata> {
  const { lang } = await params;
  if (!isLanguage(lang)) return {};
  return createMetadata({
    language: lang,
    title: lang === "zh" ? "HHY 系统脚本语言：管道即语言" : "HHY System Scripting Language: Pipe Everything",
    description: lang === "zh"
      ? `HHY 是一门开源、以 Flow 为核心的系统脚本语言，通过统一管道处理文件、进程、HTTP、JSON、CSV 与自动化任务。下载 HHY ${hhyVersion} 或在线学习语言与本地进程扩展。`
      : `HHY is an open-source, flow-first system scripting language that unifies files, processes, HTTP, JSON, CSV, and automation through pipelines. Download HHY ${hhyVersion} or learn the language and local process extensions online.`,
    keywords: lang === "zh" ? ["HHY下载", "HHY教程", "HHY语法"] : ["download HHY", "HHY tutorial", "HHY documentation"]
  });
}

export default async function LanguageHome({ params }: { params: Promise<{ lang: string }> }) {
  const { lang } = await params;
  if (!isLanguage(lang)) notFound();
  const description = lang === "zh"
    ? "以统一 Flow 管道连接文件、进程、网络与结构化数据的开源系统脚本语言。"
    : "An open-source system scripting language connecting files, processes, networks, and structured data with one Flow pipeline model.";
  return (
    <>
      <JsonLd data={[
        {
          "@context": "https://schema.org",
          "@type": "WebSite",
          name: siteName,
          url: siteOrigin,
          inLanguage: ["zh-CN", "en"],
          description
        },
        {
          "@context": "https://schema.org",
          "@type": "SoftwareApplication",
          name: "HHY Language",
          alternateName: "HHY",
          applicationCategory: "DeveloperApplication",
          applicationSubCategory: "Programming Language",
          operatingSystem: "macOS arm64, Linux arm64, Linux x86_64, Windows x86_64 MSYS2",
          softwareVersion: hhyVersion,
          license: "https://www.apache.org/licenses/LICENSE-2.0",
          codeRepository: "https://github.com/hh696-wq/hhy-vm",
          downloadUrl: releaseUrl,
          url: localizedUrl(lang),
          description,
          inLanguage: lang === "zh" ? "zh-CN" : "en"
        }
      ]} />
      <HomePage language={lang} />
    </>
  );
}
