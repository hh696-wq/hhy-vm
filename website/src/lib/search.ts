import type { DocBlock } from "./docs";
import { chapterKind, chapters } from "./docs";
import type { Language } from "./i18n";

export type SearchDocument = {
  title: string;
  chapterTitle: string;
  summary: string;
  text: string;
  href: string;
  kind: ReturnType<typeof chapterKind>;
};

function blockText(block: DocBlock): string {
  if (block.type === "p" || block.type === "note") return block.text;
  if (block.type === "code") return `${block.filename ?? ""} ${block.code}`;
  if (block.type === "terminal") return `${block.command} ${block.output}`;
  if (block.type === "terminal-card") return `${block.title} ${block.command} ${block.output} ${block.caption}`;
  if (block.type === "list") return block.items.join(" ");
  if (block.type === "table") return [...block.columns, ...block.rows.flat()].join(" ");
  if (block.type === "link") return `${block.label} ${block.description}`;
  if (block.type === "image") return `${block.alt} ${block.caption}`;
  if (block.type === "api") return block.entries.map((entry) => `${entry.name} ${entry.signature} ${entry.description}`).join(" ");
  return "";
}

export function getSearchDocuments(language: Language): SearchDocument[] {
  return chapters.flatMap((chapter) => chapter.sections[language].map((section, index) => {
    const sectionText = section.blocks.map(blockText).join(" ");
    return {
      title: section.title,
      chapterTitle: chapter.title[language],
      summary: chapter.summary[language],
      text: `${chapter.title[language]} ${chapter.summary[language]} ${section.title} ${sectionText}`,
      href: `/${language}/learn/${chapter.slug}#section-${index + 1}`,
      kind: chapterKind(chapter)
    };
  }));
}
