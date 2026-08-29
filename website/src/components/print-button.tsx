"use client";

import { Printer } from "@phosphor-icons/react";
import type { Language } from "@/lib/i18n";

export function PrintButton({ language }: { language: Language }) {
  return (
    <button className="print-button" type="button" onClick={() => window.print()}>
      <Printer size={18} weight="duotone" />
      {language === "zh" ? "打印 / 保存为 PDF" : "Print / Save as PDF"}
    </button>
  );
}
