import {
  ArrowRight,
  BookOpen,
  CheckCircle,
  CloudArrowDown,
  Cube,
  Files,
  FlowArrow,
  GitBranch,
  Monitor,
  RocketLaunch,
  ShieldCheck,
  Smiley,
  TerminalWindow
} from "@phosphor-icons/react/dist/ssr";
import Image from "next/image";
import Link from "next/link";
import type { Language } from "@/lib/i18n";
import { ui } from "@/lib/i18n";
import { CodeBlock } from "./code-block";

const heroCode = `path("./logs")
    |> files("**/*.log")
    |> where { file -> file.size > 1mib }
    |> flat_map { file -> read_lines(file.path) }
    |> where { line -> contains(line, "ERROR") }
    |> take(100)
    |> save_lines(path("errors.txt"))`;

export function HomePage({ language }: { language: Language }) {
  const copy = ui[language];
  const cards = [
    { icon: Smiley, title: copy.simple, body: copy.simpleBody },
    { icon: Cube, title: copy.units, body: copy.unitsBody },
    { icon: ShieldCheck, title: copy.safe, body: copy.safeBody }
  ];
  const lessons = language === "zh" ? [
    { icon: RocketLaunch, number: "01", title: "快速开始", body: "安装解释器，运行第一个 .hhy 脚本。", code: "hhy run hello.hhy", slug: "quick-start" },
    { icon: Files, number: "02", title: "文件与路径", body: "遍历目录、读取文本并原子写入结果。", code: "path(\"./data\") |> files(\"*.json\")", slug: "files-and-paths" },
    { icon: CloudArrowDown, number: "03", title: "进程与 HTTP", body: "运行系统命令并调用 HTTP API。", code: "http.get(url) |> timeout(5s) |> send", slug: "http" }
  ] : [
    { icon: RocketLaunch, number: "01", title: "Quick Start", body: "Install the interpreter and run your first .hhy script.", code: "hhy run hello.hhy", slug: "quick-start" },
    { icon: Files, number: "02", title: "Files and Paths", body: "Walk directories, read text, and save results atomically.", code: "path(\"./data\") |> files(\"*.json\")", slug: "files-and-paths" },
    { icon: CloudArrowDown, number: "03", title: "Processes and HTTP", body: "Run system commands and call HTTP APIs.", code: "http.get(url) |> timeout(5s) |> send", slug: "http" }
  ];

  return (
    <main>
      <section className="hero section-shell">
        <div className="hero-copy">
          <p className="eyebrow">{copy.version}</p>
          <h1>{copy.headline}</h1>
          <p className="hero-subtitle">{copy.subtitle}</p>
          <p className="hero-solo">{copy.solo}</p>
          <div className="hero-actions">
            <Link className="primary-button" href={`/${language}/learn/quick-start`}>{copy.getStarted}<ArrowRight size={19} weight="bold" /></Link>
            <Link className="secondary-button" href="https://github.com/hh696-wq/hhy-vm/blob/main/docs/HHY_V1.md" target="_blank">{copy.readSpec}<BookOpen size={19} /></Link>
          </div>
        </div>
        <div className="hero-visual" aria-hidden="true">
          <Image src="/hhy-logo.png" alt="" width={540} height={540} priority />
        </div>
      </section>

      <section className="section-shell code-showcase">
        <CodeBlock code={heroCode} filename="example.hhy" locale={language} />
      </section>

      <section className="section-shell philosophy-panel">
        <div className="philosophy-intro">
          <span className="round-icon"><TerminalWindow size={28} weight="duotone" /></span>
          <h2>{copy.what}</h2>
          <p>{copy.whatBody}</p>
        </div>
        <div className="philosophy-item">
          <span className="round-icon"><FlowArrow size={30} weight="duotone" /></span>
          <h3>{copy.flow}</h3><p>{copy.flowBody}</p>
        </div>
        <div className="connector" aria-hidden="true" />
        <div className="philosophy-item">
          <span className="round-icon"><GitBranch size={30} weight="duotone" /></span>
          <h3>{copy.pipe}</h3><p>{copy.pipeBody}</p>
        </div>
        <div className="connector" aria-hidden="true" />
        <div className="philosophy-item">
          <span className="round-icon"><Monitor size={30} weight="duotone" /></span>
          <h3>{copy.system}</h3><p>{copy.systemBody}</p>
        </div>
      </section>

      <section className="section-shell feature-grid">
        {cards.map(({ icon: Icon, title, body }) => (
          <article className="feature-card" key={title}>
            <span className="round-icon"><Icon size={30} weight="duotone" /></span>
            <div><h3>{title}</h3><p>{body}</p></div>
          </article>
        ))}
      </section>

      <section className="section-shell learn-section">
        <div className="section-heading">
          <div><p className="eyebrow">LEARN</p><h2>{copy.learnTitle}</h2><p>{copy.learnBody}</p></div>
          <Link href={`/${language}/learn`}>{copy.allTutorials}<ArrowRight size={18} /></Link>
        </div>
        <div className="lesson-grid">
          {lessons.map(({ icon: Icon, number, title, body, code, slug }) => (
            <Link className="lesson-card" href={`/${language}/learn/${slug}`} key={slug}>
              <div className="lesson-top"><span className="round-icon"><Icon size={25} weight="duotone" /></span><span>{number}</span></div>
              <h3>{title}</h3><p>{body}</p><code>{code}</code>
            </Link>
          ))}
        </div>
      </section>

      <section className="section-shell status-banner">
        <CheckCircle size={38} weight="duotone" />
        <div><strong>{copy.stable}</strong><p>{copy.stableBody}</p></div>
        <Link href="https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml" target="_blank">{language === "zh" ? "查看验证" : "View verification"}<ArrowRight size={18} /></Link>
      </section>
    </main>
  );
}
