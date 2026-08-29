"use client";

import { ArrowRight, Command, MagnifyingGlass, X } from "@phosphor-icons/react";
import Link from "next/link";
import { createPortal } from "react-dom";
import { useEffect, useMemo, useRef, useState } from "react";
import type { Language } from "@/lib/i18n";
import type { SearchDocument } from "@/lib/search";

const kindLabel = {
  zh: { guide: "指南", project: "实战项目", reference: "参考", extension: "扩展", roadmap: "路线图" },
  en: { guide: "Guide", project: "Project", reference: "Reference", extension: "Extension", roadmap: "Roadmap" }
} as const;

function normalize(value: string) {
  return value.toLocaleLowerCase().normalize("NFKC");
}

function excerpt(text: string, query: string) {
  const compact = text.replace(/\s+/g, " ").trim();
  const at = normalize(compact).indexOf(normalize(query));
  const start = Math.max(0, at < 0 ? 0 : at - 42);
  const end = Math.min(compact.length, start + 126);
  return `${start > 0 ? "…" : ""}${compact.slice(start, end)}${end < compact.length ? "…" : ""}`;
}

export function DocsSearch({ language, documents }: { language: Language; documents: SearchDocument[] }) {
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState("");
  const inputRef = useRef<HTMLInputElement>(null);
  const copy = language === "zh"
    ? { button: "搜索文档", placeholder: "搜索章节、函数和代码…", title: "搜索 HHY 文档", hint: "输入关键词开始搜索", empty: "没有找到相关内容", close: "关闭搜索", results: "个结果" }
    : { button: "Search docs", placeholder: "Search chapters, functions, and code…", title: "Search HHY docs", hint: "Type a keyword to search", empty: "No matching content", close: "Close search", results: "results" };

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") {
        event.preventDefault();
        setOpen(true);
      }
      if (event.key === "Escape") setOpen(false);
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, []);

  useEffect(() => {
    if (!open) return;
    document.body.classList.add("search-open");
    requestAnimationFrame(() => inputRef.current?.focus());
    return () => document.body.classList.remove("search-open");
  }, [open]);

  const results = useMemo(() => {
    const terms = normalize(query).split(/\s+/).filter(Boolean);
    if (!terms.length) return [];
    return documents
      .map((document) => {
        const haystack = normalize(document.text);
        if (!terms.every((term) => haystack.includes(term))) return null;
        const title = normalize(`${document.chapterTitle} ${document.title}`);
        const score = terms.reduce((total, term) => total + (title.includes(term) ? 4 : 1), 0);
        return { document, score };
      })
      .filter((result): result is { document: SearchDocument; score: number } => result !== null)
      .sort((a, b) => b.score - a.score)
      .slice(0, 12);
  }, [documents, query]);

  return (
    <>
      <button className="docs-search-trigger header" type="button" onClick={() => setOpen(true)} aria-haspopup="dialog" aria-label={copy.button}>
        <MagnifyingGlass size={18} />
        <span>{copy.button}</span>
        <kbd><Command size={12} />K</kbd>
      </button>
      {open ? createPortal(
        <div className="search-backdrop" role="presentation" onMouseDown={(event) => { if (event.target === event.currentTarget) setOpen(false); }}>
          <section className="search-dialog" role="dialog" aria-modal="true" aria-label={copy.title}>
            <header className="search-input-row">
              <MagnifyingGlass size={22} aria-hidden />
              <input ref={inputRef} value={query} onChange={(event) => setQuery(event.target.value)} placeholder={copy.placeholder} aria-label={copy.title} />
              <button type="button" onClick={() => setOpen(false)} aria-label={copy.close}><X size={21} /></button>
            </header>
            <div className="search-results" aria-live="polite">
              {!query.trim() ? <div className="search-state"><MagnifyingGlass size={30} />{copy.hint}</div> : null}
              {query.trim() && !results.length ? <div className="search-state"><MagnifyingGlass size={30} />{copy.empty}</div> : null}
              {results.length ? <p className="search-count">{results.length} {copy.results}</p> : null}
              {results.map(({ document }) => (
                <Link href={document.href} key={document.href} onClick={() => setOpen(false)}>
                  <div className="search-result-meta"><span>{kindLabel[language][document.kind]}</span><span>{document.chapterTitle}</span></div>
                  <strong>{document.title}</strong>
                  <p>{excerpt(document.text, query)}</p>
                  <ArrowRight size={18} aria-hidden />
                </Link>
              ))}
            </div>
            <footer><kbd>ESC</kbd> {language === "zh" ? "关闭" : "Close"}<span><kbd>↵</kbd> {language === "zh" ? "打开结果" : "Open result"}</span></footer>
          </section>
        </div>,
        document.body
      ) : null}
    </>
  );
}
