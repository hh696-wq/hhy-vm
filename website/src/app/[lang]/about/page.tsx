import type { Metadata } from "next";
import {
  ArrowUpRight,
  CheckCircle,
  Code,
  DownloadSimple,
  FlowArrow,
  GitBranch,
  GlobeHemisphereWest,
  Package,
  RocketLaunch,
  ShieldCheck,
  Sparkle
} from "@phosphor-icons/react/dist/ssr";
import Image from "next/image";
import Link from "next/link";
import { notFound } from "next/navigation";
import { JsonLd } from "@/components/json-ld";
import { isLanguage } from "@/lib/i18n";
import { hhyAboutMilestones, hhyReleaseUrl, hhyVersionTag } from "@/lib/release";
import { createMetadata, localizedUrl, siteName } from "@/lib/seo";

const content = {
  zh: {
    eyebrow: "品牌与理念",
    title: "关于 HHY",
    lead: "一门以 Flow 为语义中心、为真实系统自动化而构建的开源脚本语言。",
    statement: "让文件、进程、网络与结构化数据沿同一种可读、可控的路径流动。",
    whyTitle: "为什么要做 HHY",
    whyBody: "系统脚本经常在 shell、文本工具、JSON 处理器与通用语言之间来回切换。HHY 希望把这些任务收拢到一套一致的值模型、错误模型和管道语义中：既保留脚本语言的直接，也把资源边界、取消和结构化诊断变成语言的一部分。",
    principlesTitle: "三条不轻易改变的原则",
    principles: [
      ["Flow 是语义，不是装饰", "管道不是语法糖；Value、Stream、Error 与取消共同构成从左到右的执行模型。"],
      ["可预测优先于魔法", "确定的 grammar、显式能力、结构化错误和有界资源，比隐藏的便利更重要。"],
      ["证据先于架构承诺", "性能优化、Runtime 拆分和 ABI 开放都由 benchmark、Profiler 与真实集成证据驱动。"]
    ],
    progressEyebrow: "当前进度",
    progressTitle: `${hhyVersionTag} 已正式发布`,
    progressBody: "核心语言语义已经冻结，四平台发行证据持续验证；官方扩展已进入带签名、按 target 分发的 Registry 阶段。",
    brandTitle: "品牌名称与标志",
    brandBody: "正式名称是 HHY Language，日常简称 HHY。品牌不人为扩写 HHY 的含义；它代表的是一套持续演进、以 Flow 为核心的语言实践。主标志由字母 H、连续流线与向前箭头组成，表达连接、流动与推进。",
    brandRules: ["优先使用完整彩色标志", "标志四周保留足够留白", "不要拉伸、旋转或重新着色", "小尺寸场景使用站点现有图标版本"],
    downloadLogo: "下载 PNG 标志",
    linksEyebrow: "链接",
    linksTitle: "项目与生态",
    linksBody: "这里汇集 HHY 的官方入口和直接使用的开源技术。列入生态不表示对方为 HHY 背书。",
    links: [
      ["GitHub", "源代码、Issue 与贡献记录", "https://github.com/hh696-wq/hhy-vm", "code"],
      ["官方 Registry", "签名扩展索引与信任根", "https://registry.hhylang.dev", "package"],
      ["Releases", "四平台归档与 SHA-256", hhyReleaseUrl, "release"],
      ["Lexbor", "HTML 扩展使用的解析引擎", "https://lexbor.com", "web"]
    ],
    roadmap: "查看语言与 VM 路线图"
  },
  en: {
    eyebrow: "Brand and philosophy",
    title: "About HHY",
    lead: "An open-source scripting language built around Flow semantics for real system automation.",
    statement: "Let files, processes, networks, and structured data move through one readable, controlled path.",
    whyTitle: "Why HHY exists",
    whyBody: "System scripts often jump between a shell, text tools, JSON processors, and a general-purpose language. HHY brings those jobs into one consistent value, error, and pipeline model—keeping scripting direct while making resource boundaries, cancellation, and structured diagnostics part of the language.",
    principlesTitle: "Three principles we protect",
    principles: [
      ["Flow is semantics, not decoration", "Pipes are not syntax sugar: Value, Stream, Error, and cancellation form one left-to-right execution model."],
      ["Predictability over magic", "A deterministic grammar, explicit capabilities, structured errors, and bounded resources matter more than hidden convenience."],
      ["Evidence before architecture promises", "Performance work, Runtime extraction, and any ABI decision follow benchmarks, profiling, and real integration evidence."]
    ],
    progressEyebrow: "Current progress",
    progressTitle: `${hhyVersionTag} is officially released`,
    progressBody: "Core language semantics are frozen, four-platform release evidence remains continuously verified, and official extensions now ship through a signed, target-aware Registry.",
    brandTitle: "Name and mark",
    brandBody: "The formal name is HHY Language; HHY is the everyday short name. We do not invent an expanded phrase for HHY. It stands for an evolving, Flow-first language practice. The primary mark combines the letter H, a continuous flow, and a forward arrow to express connection, movement, and progress.",
    brandRules: ["Prefer the complete full-color mark", "Keep generous clear space around it", "Do not stretch, rotate, or recolor it", "Use the existing site icon at small sizes"],
    downloadLogo: "Download PNG mark",
    linksEyebrow: "Links",
    linksTitle: "Project and ecosystem",
    linksBody: "Official HHY destinations and open-source technology used directly by the project. Inclusion does not imply endorsement of HHY.",
    links: [
      ["GitHub", "Source, issues, and contribution history", "https://github.com/hh696-wq/hhy-vm", "code"],
      ["Official Registry", "Signed extension index and trust root", "https://registry.hhylang.dev", "package"],
      ["Releases", "Four-platform archives and SHA-256 files", hhyReleaseUrl, "release"],
      ["Lexbor", "The parsing engine used by the HTML extension", "https://lexbor.com", "web"]
    ],
    roadmap: "View the Language and VM roadmap"
  }
} as const;

const linkIcons = { code: Code, package: Package, release: RocketLaunch, web: GlobeHemisphereWest } as const;

export async function generateMetadata({ params }: { params: Promise<{ lang: string }> }): Promise<Metadata> {
  const { lang } = await params;
  if (!isLanguage(lang)) return {};
  const copy = content[lang];
  return createMetadata({
    language: lang,
    path: "/about",
    title: `${copy.title} — HHY Language`,
    description: copy.lead,
    keywords: lang === "zh" ? ["HHY品牌", "HHY理念", "HHY开源"] : ["HHY brand", "HHY philosophy", "HHY open source"]
  });
}

export default async function AboutPage({ params }: { params: Promise<{ lang: string }> }) {
  const { lang } = await params;
  if (!isLanguage(lang)) notFound();
  const copy = content[lang];
  const milestones = hhyAboutMilestones[lang];

  return <main className="about-page">
    <JsonLd data={{
      "@context": "https://schema.org",
      "@type": "AboutPage",
      name: copy.title,
      description: copy.lead,
      url: localizedUrl(lang, "/about"),
      inLanguage: lang === "zh" ? "zh-CN" : "en",
      about: { "@type": "SoftwareApplication", name: siteName, url: localizedUrl(lang) }
    }} />

    <section className="about-hero section-shell">
      <div className="about-hero-copy">
        <p className="eyebrow">{copy.eyebrow}</p>
        <h1>{copy.title}</h1>
        <p className="about-lead">{copy.lead}</p>
        <blockquote>{copy.statement}</blockquote>
        <div className="about-hero-actions">
          <Link className="primary-button" href={`/${lang}/learn/quick-start`}>{lang === "zh" ? "开始使用" : "Get started"}<ArrowUpRight size={18} /></Link>
          <Link className="secondary-button" href={hhyReleaseUrl} target="_blank">{lang === "zh" ? "下载最新版" : "Download latest"}</Link>
        </div>
      </div>
      <div className="about-mark-stage">
        <span>{hhyVersionTag} · {lang === "zh" ? "已发布" : "released"}</span>
        <Image src="/hhy-logo.png" alt="HHY Language logo" width={1254} height={1254} priority />
      </div>
    </section>

    <section className="about-story section-shell">
      <div className="about-story-intro">
        <span className="about-number">01</span>
        <div><p className="eyebrow">Origin</p><h2>{copy.whyTitle}</h2></div>
      </div>
      <p className="about-story-body">{copy.whyBody}</p>
    </section>

    <section className="about-principles section-shell">
      <div className="about-section-heading"><Sparkle size={24} /><h2>{copy.principlesTitle}</h2></div>
      <div className="about-principle-grid">
        {copy.principles.map(([title, body], index) => {
          const Icon = [FlowArrow, ShieldCheck, GitBranch][index];
          return <article key={title}><div className="round-icon"><Icon size={26} /></div><span>0{index + 1}</span><h3>{title}</h3><p>{body}</p></article>;
        })}
      </div>
    </section>

    <section className="about-progress">
      <div className="section-shell">
        <div className="about-progress-heading">
          <div><p className="eyebrow">{copy.progressEyebrow}</p><h2>{copy.progressTitle}</h2></div>
          <p>{copy.progressBody}</p>
        </div>
        <div className="about-milestones">
          {milestones.map(({ version, title, body, status }) => <article className={status === "current" ? "current" : undefined} key={version}>
            <div><CheckCircle weight={status === "planned" ? "regular" : "fill"} size={21} /><strong>{version}</strong></div>
            <h3>{title}</h3><p>{body}</p>
          </article>)}
        </div>
        <Link className="about-inline-link" href={`/${lang}/learn/language-vm-roadmap`}>{copy.roadmap}<ArrowUpRight size={17} /></Link>
      </div>
    </section>

    <section className="about-brand section-shell">
      <div className="about-brand-visual"><Image src="/hhy-logo.png" alt="HHY Language full-color brand mark" width={1254} height={1254} /></div>
      <div className="about-brand-copy"><p className="eyebrow">Identity</p><h2>{copy.brandTitle}</h2><p>{copy.brandBody}</p>
        <ul>{copy.brandRules.map((rule) => <li key={rule}><CheckCircle size={19} />{rule}</li>)}</ul>
        <a className="secondary-button" href="/hhy-logo.png" download="hhy-language-logo.png"><DownloadSimple size={19} />{copy.downloadLogo}</a>
      </div>
    </section>

    <section className="about-links section-shell">
      <div className="about-links-heading"><div><p className="eyebrow">{copy.linksEyebrow}</p><h2>{copy.linksTitle}</h2></div><p>{copy.linksBody}</p></div>
      <div className="about-link-grid">{copy.links.map(([title, description, href, icon]) => {
        const Icon = linkIcons[icon];
        return <Link href={href} target="_blank" rel="noreferrer" key={title}><div className="round-icon"><Icon size={25} /></div><div><h3>{title}</h3><p>{description}</p></div><ArrowUpRight size={20} /></Link>;
      })}</div>
    </section>
  </main>;
}
