"use client";

import { Check, Copy, FileCode } from "@phosphor-icons/react";
import { useState } from "react";
import { HhyCode } from "@/components/hhy-code";
import type { Language } from "@/lib/i18n";

type CodeBlockProps = {
  code: string;
  language?: string;
  filename?: string;
  locale: Language;
  compact?: boolean;
};

export function CodeBlock({ code, language = "hhy", filename, locale, compact }: CodeBlockProps) {
  const [copied, setCopied] = useState(false);

  async function copyCode() {
    await navigator.clipboard.writeText(code);
    setCopied(true);
    window.setTimeout(() => setCopied(false), 1600);
  }

  return (
    <div className={`code-window${compact ? " code-window-compact" : ""}`}>
      <div className="code-toolbar">
        <span className="code-filename">
          <FileCode size={18} weight="duotone" />
          {filename ?? language}
        </span>
        <button className="copy-button" type="button" onClick={copyCode} aria-label={locale === "zh" ? "复制代码" : "Copy code"}>
          {copied ? <Check size={17} weight="bold" /> : <Copy size={17} />}
          {copied ? (locale === "zh" ? "已复制" : "Copied") : locale === "zh" ? "复制" : "Copy"}
        </button>
      </div>
      <pre className={language === "hhy" ? "hhy-source" : undefined} data-language={language}>
        <code>{language === "hhy" ? <HhyCode code={code} /> : code}</code>
      </pre>
    </div>
  );
}
