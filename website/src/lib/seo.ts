import type { Metadata } from "next";
import type { Language } from "@/lib/i18n";
import { hhyReleaseUrl } from "@/lib/release";

export const siteOrigin = "https://hhylang.dev";
export const siteName = "HHY Language";
export const releaseUrl = hhyReleaseUrl;

const sharedKeywords = [
  "HHY", "HHY Language", "HHY scripting language", "flow-first language",
  "pipeline language", "system scripting", "system automation", "open source language"
];

const localizedKeywords: Record<Language, string[]> = {
  zh: ["HHY语言", "管道脚本语言", "数据流语言", "系统脚本语言", "系统自动化", "文件处理", "进程管理", "HTTP脚本", "JSON处理", "C语言解释器"],
  en: ["automation scripting language", "data pipeline scripting", "file automation", "process automation", "HTTP scripting", "JSON processing", "C interpreter"]
};

export function localizedUrl(language: Language, path = "") {
  return `${siteOrigin}/${language}${path}`;
}

export function pageAlternates(language: Language, path = ""): Metadata["alternates"] {
  return {
    canonical: localizedUrl(language, path),
    languages: {
      "zh-CN": localizedUrl("zh", path),
      en: localizedUrl("en", path),
      "x-default": localizedUrl("en", path)
    }
  };
}

type SeoInput = {
  language: Language;
  path?: string;
  title: string;
  description: string;
  keywords?: string[];
  type?: "website" | "article";
};

export function createMetadata({ language, path = "", title, description, keywords = [], type = "website" }: SeoInput): Metadata {
  const url = localizedUrl(language, path);
  const locale = language === "zh" ? "zh_CN" : "en_US";
  return {
    title,
    description,
    keywords: [...sharedKeywords, ...localizedKeywords[language], ...keywords],
    alternates: pageAlternates(language, path),
    category: "technology",
    openGraph: {
      type,
      url,
      title,
      description,
      siteName,
      locale,
      alternateLocale: language === "zh" ? ["en_US"] : ["zh_CN"],
      images: [{ url: "/hhy-logo.png", width: 1254, height: 1254, alt: "HHY Language pipeline logo" }]
    },
    twitter: {
      card: "summary",
      title,
      description,
      images: ["/hhy-logo.png"]
    }
  };
}
