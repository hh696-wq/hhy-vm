import type { Metadata } from "next";
import { notFound } from "next/navigation";
import { CodeBlock } from "@/components/code-block";
import { LearnLayout } from "@/components/learn-layout";
import { chapters, getChapter } from "@/lib/docs";
import { isLanguage, languages } from "@/lib/i18n";

export function generateStaticParams() {
  return languages.flatMap((lang) => chapters.map((chapter) => ({ lang, slug: chapter.slug })));
}

export async function generateMetadata({ params }: { params: Promise<{ lang: string; slug: string }> }): Promise<Metadata> {
  const { lang, slug } = await params;
  const chapter = getChapter(slug);
  if (!isLanguage(lang) || !chapter) return {};
  return { title: chapter.title[lang], description: chapter.summary[lang] };
}

export default async function ChapterPage({ params }: { params: Promise<{ lang: string; slug: string }> }) {
  const { lang, slug } = await params;
  const chapter = getChapter(slug);
  if (!isLanguage(lang) || !chapter) notFound();

  return (
    <LearnLayout language={lang} chapter={chapter}>
      <article className="chapter-article">
        <p className="eyebrow">{lang === "zh" ? `第 ${chapter.order} 章` : `Chapter ${chapter.order}`}</p>
        <h1>{chapter.title[lang]}</h1>
        <p className="chapter-summary">{chapter.summary[lang]}</p>
        {chapter.sections[lang].map((section) => (
          <section key={section.title}>
            <h2>{section.title}</h2>
            {section.blocks.map((block, index) => {
              if (block.type === "p") return <p key={index}>{block.text}</p>;
              if (block.type === "note") return <aside className="doc-note" key={index}>{block.text}</aside>;
              if (block.type === "list") return <ul key={index}>{block.items.map((item) => <li key={item}>{item}</li>)}</ul>;
              return <CodeBlock code={block.code} language={block.language} filename={block.language === "hhy" ? "example.hhy" : undefined} locale={lang} compact key={index} />;
            })}
          </section>
        ))}
      </article>
    </LearnLayout>
  );
}
