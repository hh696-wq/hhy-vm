import { AppleLogo, ArrowRight, CheckCircle, Code, Cube, DownloadSimple, FileText, FlowArrow, GlobeHemisphereWest, Graph, LinuxLogo, ShieldCheck, TerminalWindow, UsersThree, WaveSine, WindowsLogo } from "@phosphor-icons/react/dist/ssr";
import Image from "next/image";
import Link from "next/link";
import type { Language } from "@/lib/i18n";
import { ui } from "@/lib/i18n";
import { hhyCoreCallableCount, hhyCurrentRelease, hhyVersionLabel, hhyVersionTag } from "@/lib/release";
import { QuickInstall } from "@/components/quick-install";

const siteGraphSnippet = `1  try {
2      let config = read_text(path(args[0])) |> parse_json
3      let crawl_result = crawl(config)
4      let inventory = records(crawl_result)
5      let graph = build_graph(crawl_result.pages, config)
6      let report = build_report(
7          config.project, crawl_result,
8          inventory, graph, failures(crawl_result)
9      )
10     # ... 原子写入 inventory、graph 与 failures ...
11     report |> encode_json({ pretty: true })
12         |> save_text(path(args[3]), { atomic: true })
13     if not report.ok { exit(1) }
14 } catch err { print_error(err); exit(1) }`;

export function HomePage({ language }: { language: Language }) {
  const copy = ui[language];
  const release = hhyCurrentRelease[language];
  const zh = language === "zh";
  const flow = [
    { label: "URL", icon: GlobeHemisphereWest },
    { label: "Frontier", icon: Graph },
    { label: "Fetch", icon: DownloadSimple },
    { label: "Parse", icon: Code },
    { label: "Report", icon: FileText }
  ];
  const principles = [
    { name: "Flow", description: zh ? "统一 Stream 模型" : "One Stream model", icon: WaveSine },
    { name: "Pipe", description: zh ? "组合核心" : "The composition core", icon: FlowArrow },
    { name: "System", description: zh ? "系统能力是一等值" : "System capabilities as values", icon: Cube }
  ];

  return (
    <main className="editorial-home">
      <section className="editorial-hero section-shell">
        <div className="editorial-intro">
          <p className="eyebrow">{copy.version}</p>
          <h1>{copy.headline}</h1>
          <h2>{zh ? "面向系统自动化的数据流脚本语言" : "A flow-first scripting language for system automation"}</h2>
          <p>{copy.whatBody}</p>
          <div className="editorial-actions">
            <Link className="primary-button" href={`/${language}/learn/quick-start`}>{copy.getStarted}<ArrowRight size={18} weight="bold" /></Link>
            <Link className="secondary-button" href="https://github.com/hh696-wq/hhy-vm/releases/latest" target="_blank"><DownloadSimple size={18} />{zh ? `安装 ${hhyVersionLabel}` : `Install ${hhyVersionLabel}`}</Link>
          </div>
          <QuickInstall language={language} />
          <div className="language-principles" aria-label={zh ? "HHY 语言原则" : "HHY language principles"}>
            {principles.map(({ name, description, icon: Icon }) => (
              <div className="language-principle" key={name}>
                <Icon size={27} weight="duotone" aria-hidden />
                <span><strong>{name}</strong><small>{description}</small></span>
              </div>
            ))}
          </div>
        </div>

        <article className="featured-project">
          <div className="featured-heading">
            <div><p className="eyebrow">{zh ? "本期精选" : "Featured"}</p><h2>{zh ? "用 HHY 构建 SiteGraph Auditor" : "Build SiteGraph Auditor with HHY"}</h2></div>
          </div>
          <p>{zh ? "从 URL 出发，发现站点结构、抓取内容、解析信息、生成审计报告。HHY 以数据流让每一步都可组合、可复用、可验证。" : "Start from a URL, discover site structure, fetch content, parse metadata, and produce an audit report. HHY keeps every step composable, reusable, and verifiable."}</p>
          <div className="project-flow" aria-label={zh ? "SiteGraph Auditor 执行流程" : "SiteGraph Auditor flow"}>
            {flow.map(({ label, icon: Icon }, index) => <div className="project-flow-item" key={label}><span><Icon size={21} weight="duotone" />{label}</span>{index < flow.length - 1 ? <ArrowRight size={20} aria-hidden /> : null}</div>)}
          </div>
          <pre className="project-output" aria-label={zh ? "SiteGraph Auditor HHY 源码示例" : "SiteGraph Auditor HHY source example"}><code>{siteGraphSnippet}</code></pre>
          <div className="project-verification">
            <span className="verification-label"><ShieldCheck size={19} weight="duotone" />{zh ? "验证通过" : "Verified"}</span>
            <span className="verified-platform"><AppleLogo size={19} weight="fill" /><span>macOS</span><CheckCircle size={15} weight="fill" /></span>
            <span className="verified-platform"><LinuxLogo size={19} weight="fill" /><span>Linux</span><CheckCircle size={15} weight="fill" /></span>
            <span className="verified-platform"><WindowsLogo size={19} weight="fill" /><span>Windows</span><CheckCircle size={15} weight="fill" /></span>
          </div>
        </article>
      </section>

      <section className="editorial-grid section-shell">
        <article className="release-story">
          <div className="story-label"><CheckCircle size={18} weight="duotone" />{zh ? "当前版本" : "Current release"}</div>
          <h2>{hhyVersionTag} · {release.title}</h2>
          <p>{release.summary}</p>
          <div className="release-art"><Image src="/hhy-logo.png" alt="" width={420} height={280} /></div>
          <Link href={`/${language}/learn/language-vm-roadmap`}>{zh ? "查看版本路线图" : "View release roadmap"}<ArrowRight size={17} /></Link>
        </article>

        <div className="editorial-stack">
          <article className="editorial-story">
            <div className="story-label"><Code size={18} weight="duotone" />{zh ? "参考" : "Reference"}</div>
            <h2>{zh ? `完整的 ${hhyCoreCallableCount} 个核心 callable` : `All ${hhyCoreCallableCount} core callables`}</h2>
            <p>{zh ? "来自 Runtime Callable Contract Registry 的完整签名与用途。" : "Complete signatures and purposes from the Runtime Callable Contract Registry."}</p>
            <Link href={`/${language}/learn/standard-library`}>{zh ? "浏览函数索引" : "Browse function index"}<ArrowRight size={16} /></Link>
          </article>
          <article className="editorial-story reference-story">
            <div className="story-label"><Code size={18} weight="duotone" />{zh ? "编辑器支持" : "Editor support"}</div>
            <h2>{zh ? "为 .hhy 文件准备好高亮" : "Syntax support for .hhy files"}</h2>
            <p>{zh ? "同一语法源生成 VS Code 与 Sublime Text 插件，包含高亮、注释、括号、缩进与常用片段。" : "One syntax source generates VS Code and Sublime Text packages with highlighting, comments, brackets, indentation, and snippets."}</p>
            <Link href={`/${language}/learn/editor-support`}>{zh ? "安装语言包" : "Install language support"}<ArrowRight size={16} /></Link>
          </article>
        </div>

        <article className="community-story">
          <div className="story-label"><UsersThree size={19} weight="duotone" />{zh ? "参与社区" : "Join the community"}</div>
          <h2>{zh ? "在 GitHub 提问、分享与贡献" : "Ask, share, and contribute on GitHub"}</h2>
          <p>{zh ? "阅读源码、提交问题，或为语言、文档与实战项目贡献改进。" : "Read the source, open an issue, or improve the language, documentation, and practical projects."}</p>
          <nav aria-label={zh ? "社区入口" : "Community links"}>
            <Link href="https://github.com/hh696-wq/hhy-vm/issues" target="_blank"><TerminalWindow size={20} /><span><strong>{zh ? "提出问题" : "Open an issue"}</strong><small>{zh ? "反馈问题与建议" : "Report bugs and ideas"}</small></span><ArrowRight size={16} /></Link>
            <Link href="https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects" target="_blank"><Graph size={20} /><span><strong>{zh ? "查看实战项目" : "Explore projects"}</strong><small>{zh ? "学习经过自测的完整应用" : "Study complete self-tested apps"}</small></span><ArrowRight size={16} /></Link>
            <Link href="https://github.com/hh696-wq/hhy-vm" target="_blank"><Code size={20} weight="duotone" /><span><strong>{zh ? "参与开发" : "Contribute"}</strong><small>{zh ? "阅读源码并参与改进" : "Read and improve the source"}</small></span><ArrowRight size={16} /></Link>
          </nav>
          <Link className="github-community-button" href="https://github.com/hh696-wq/hhy-vm" target="_blank"><Code size={20} weight="duotone" />{zh ? "访问 GitHub 仓库" : "Visit GitHub repository"}<ArrowRight size={17} /></Link>
        </article>
      </section>

      <section className="platform-proof section-shell">
        <GlobeHemisphereWest size={22} weight="duotone" />
        <div><strong>{copy.stable}</strong><p>{copy.stableBody}</p></div>
        <Link href="https://github.com/hh696-wq/hhy-vm/actions/workflows/ci.yml" target="_blank">{zh ? "查看验证" : "View verification"}<ArrowRight size={17} /></Link>
      </section>
    </main>
  );
}
