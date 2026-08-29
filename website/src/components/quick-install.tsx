"use client";

import { Check, Copy } from "@phosphor-icons/react";
import { useState } from "react";
import type { Language } from "@/lib/i18n";

const installCommand = "curl -fsSL https://hhylang.dev/install.sh | sh";

export function QuickInstall({ language }: { language: Language }) {
  const [copied, setCopied] = useState(false);
  const zh = language === "zh";

  async function copyInstallCommand() {
    await navigator.clipboard.writeText(installCommand);
    setCopied(true);
    window.setTimeout(() => setCopied(false), 1600);
  }

  return (
    <div className="quick-install">
      <span>{zh ? "快速上手" : "Quick start"}</span>
      <code>{installCommand}</code>
      <button type="button" onClick={copyInstallCommand} aria-label={zh ? "复制安装命令" : "Copy install command"}>
        {copied ? <Check size={16} weight="bold" /> : <Copy size={16} />}
        <span>{copied ? (zh ? "已复制" : "Copied") : zh ? "复制" : "Copy"}</span>
      </button>
    </div>
  );
}
