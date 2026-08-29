import { access, mkdir, mkdtemp, rm, stat } from "node:fs/promises";
import { spawn } from "node:child_process";
import { once } from "node:events";
import os from "node:os";
import path from "node:path";
import process from "node:process";

const language = process.argv[2];
if (!['zh', 'en'].includes(language)) {
  console.error("Usage: node scripts/generate-pdf.mjs <zh|en>");
  process.exit(1);
}

const root = process.cwd();
const serverFile = path.join(root, ".next", "standalone", "server.js");
await access(serverFile).catch(() => {
  console.error("Missing production build. Run `npm run build` first.");
  process.exit(1);
});

const candidates = process.platform === "darwin" ? [
  "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
  "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
  "/Applications/Chromium.app/Contents/MacOS/Chromium"
] : process.platform === "win32" ? [
  path.join(process.env.PROGRAMFILES ?? "", "Google", "Chrome", "Application", "chrome.exe"),
  path.join(process.env["PROGRAMFILES(X86)"] ?? "", "Microsoft", "Edge", "Application", "msedge.exe")
] : ["/usr/bin/google-chrome", "/usr/bin/chromium", "/usr/bin/chromium-browser"];

let browser;
for (const candidate of candidates) {
  try { await access(candidate); browser = candidate; break; } catch { /* try next */ }
}
if (!browser) {
  console.error("Chrome, Edge, or Chromium was not found. Install one before generating the PDF.");
  process.exit(1);
}

const port = process.env.PDF_PORT ?? "9810";
const outputDir = path.join(root, "output", "pdf");
const outputFile = path.join(outputDir, `hhy-language-manual-${language}.pdf`);
const profileDir = await mkdtemp(path.join(os.tmpdir(), "hhy-pdf-"));
await mkdir(outputDir, { recursive: true });
await rm(outputFile, { force: true });

const server = spawn(process.execPath, [serverFile], {
  cwd: root,
  env: { ...process.env, PORT: port, HOSTNAME: "127.0.0.1" },
  stdio: ["ignore", "pipe", "pipe"]
});

async function waitForPage() {
  const url = `http://127.0.0.1:${port}/${language}/learn/print`;
  for (let attempt = 0; attempt < 60; attempt += 1) {
    try { const response = await fetch(url); if (response.ok) return url; } catch { /* server is starting */ }
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
  throw new Error(`Print page did not become ready on port ${port}.`);
}

try {
  const url = await waitForPage();
  const chrome = spawn(browser, [
    "--headless=new",
    "--disable-gpu",
    "--no-sandbox",
    `--user-data-dir=${profileDir}`,
    "--no-pdf-header-footer",
    "--run-all-compositor-stages-before-draw",
    "--virtual-time-budget=3000",
    `--print-to-pdf=${outputFile}`,
    url
  ], { stdio: "ignore" });

  let previousSize = -1;
  let stableChecks = 0;
  for (let attempt = 0; attempt < 120; attempt += 1) {
    await new Promise((resolve) => setTimeout(resolve, 250));
    try {
      const { size } = await stat(outputFile);
      stableChecks = size > 0 && size === previousSize ? stableChecks + 1 : 0;
      previousSize = size;
      if (stableChecks >= 3) break;
    } catch { /* PDF is still being created */ }
  }
  if (stableChecks < 3) throw new Error("Browser did not finish writing the PDF within 30 seconds.");
  chrome.kill("SIGTERM");
  await Promise.race([once(chrome, "exit"), new Promise((resolve) => setTimeout(resolve, 3000))]);
  if (chrome.exitCode === null) chrome.kill("SIGKILL");
  console.log(`Generated ${path.relative(root, outputFile)}`);
} finally {
  server.kill("SIGTERM");
  for (let attempt = 0; attempt < 5; attempt += 1) {
    try { await rm(profileDir, { recursive: true, force: true }); break; }
    catch (error) {
      if (attempt === 4) throw error;
      await new Promise((resolve) => setTimeout(resolve, 250));
    }
  }
}
