import { execFileSync } from "node:child_process";
import { copyFileSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { createRequire } from "node:module";

const websiteRoot = resolve(import.meta.dirname, "..");
const repositoryRoot = resolve(websiteRoot, "..");
const outputRoot = resolve(repositoryRoot, "gitbook", "zh");
const compileRoot = mkdtempSync(join(tmpdir(), "hhy-gitbook-"));
const require = createRequire(import.meta.url);

execFileSync(resolve(websiteRoot, "node_modules", ".bin", "tsc"), [
  resolve(websiteRoot, "src", "lib", "docs.ts"),
  resolve(websiteRoot, "src", "lib", "release.ts"),
  resolve(websiteRoot, "src", "lib", "i18n.ts"),
  "--module", "commonjs",
  "--moduleResolution", "node",
  "--target", "es2022",
  "--esModuleInterop",
  "--skipLibCheck",
  "--outDir", compileRoot
], { stdio: "inherit" });

const { chapters } = require(join(compileRoot, "docs.js"));

function escapeTable(value) {
  return String(value).replaceAll("|", "\\|").replaceAll("\n", "<br>");
}

function renderBlock(block, chapter) {
  switch (block.type) {
    case "p": return `${block.text}\n`;
    case "note": return `{% hint style="info" %}\n${block.text}\n{% endhint %}\n`;
    case "code": return `${block.filename ? `**${block.filename}**\n\n` : ""}\`\`\`${block.language}\n${block.code}\n\`\`\`\n`;
    case "terminal": return `\`\`\`console\n$ ${block.command}\n${block.output}\n\`\`\`\n`;
    case "terminal-card": return `### ${block.title}\n\n\`\`\`console\n$ ${block.command}\n${block.output}\n\`\`\`\n\n${block.caption}\n`;
    case "list": return `${block.items.map((item) => `- ${item}`).join("\n")}\n`;
    case "table": {
      const header = `| ${block.columns.map(escapeTable).join(" | ")} |`;
      const divider = `| ${block.columns.map(() => "---").join(" | ")} |`;
      const rows = block.rows.map((row) => `| ${row.map(escapeTable).join(" | ")} |`).join("\n");
      return `${header}\n${divider}\n${rows}\n`;
    }
    case "link": return `[${block.label}](${block.href})\n\n${block.description}\n`;
    case "image": return `![${block.alt}](https://hhylang.dev${block.src})\n\n_${block.caption}_\n`;
    case "api": return block.entries.map((entry) => `### \`${entry.name}\`\n\n\`\`\`text\n${entry.signature}\n\`\`\`\n\n${entry.description}`).join("\n\n") + "\n";
    case "extension-flow":
    case "evolution-roadmap":
    case "runtime-performance-roadmap":
      return `{% hint style="info" %}\n本节的交互式图表请在 [hhylang.dev](https://hhylang.dev/zh/learn/${chapter.slug}) 查看。\n{% endhint %}\n`;
    default: throw new Error(`Unsupported documentation block: ${block.type}`);
  }
}

rmSync(outputRoot, { recursive: true, force: true });
mkdirSync(resolve(outputRoot, ".gitbook", "assets"), { recursive: true });

const ordered = [...chapters].sort((left, right) => left.order - right.order);
const summary = ["# 目录", "", "* [HHY 语言手册](README.md)"];

for (const chapter of ordered) {
  const lines = [
    `# ${chapter.order}. ${chapter.title.zh}`,
    "",
    chapter.summary.zh,
    ""
  ];
  for (const [index, section] of chapter.sections.zh.entries()) {
    lines.push(`## ${chapter.order}.${index + 1} ${section.title}`, "");
    for (const block of section.blocks) lines.push(renderBlock(block, chapter), "");
  }
  const filename = `${String(chapter.order).padStart(2, "0")}-${chapter.slug}.md`;
  writeFileSync(resolve(outputRoot, filename), lines.join("\n").replace(/\n{4,}/g, "\n\n\n").trimEnd() + "\n");
  summary.push(`  * [${chapter.order}. ${chapter.title.zh}](${filename})`);
}

const packageJson = JSON.parse(readFileSync(resolve(websiteRoot, "package.json"), "utf8"));
writeFileSync(resolve(outputRoot, "README.md"), `# HHY 语言手册\n\n![HHY Language](.gitbook/assets/hhy-logo.png)\n\nHHY 是一门以 Flow 为语义中心、面向真实系统自动化的开源脚本语言。\n\n本手册同步自 [hhylang.dev 中文打印版](https://hhylang.dev/zh/learn/print)，对应网站版本 **v${packageJson.version}**。\n\n- [快速开始](01-quick-start.md)\n- [GitHub 仓库](https://github.com/hh696-wq/hhy-vm)\n- [下载正式版本](https://github.com/hh696-wq/hhy-vm/releases)\n`);
writeFileSync(resolve(outputRoot, "SUMMARY.md"), summary.join("\n") + "\n");
copyFileSync(resolve(websiteRoot, "public", "hhy-logo.png"), resolve(outputRoot, ".gitbook", "assets", "hhy-logo.png"));
writeFileSync(resolve(repositoryRoot, ".gitbook.yaml"), "root: ./gitbook/zh/\n\nstructure:\n  readme: README.md\n  summary: SUMMARY.md\n");

rmSync(compileRoot, { recursive: true, force: true });
console.log(`Generated ${ordered.length} GitBook chapters in ${outputRoot}`);
