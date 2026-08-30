#!/usr/bin/env node

import dns from "node:dns/promises";
import net from "node:net";
import process from "node:process";
import { chromium } from "playwright";

const [target, domainsJson, allowPrivateJson] = process.argv.slice(2);
if (!target || !domainsJson) {
  console.error("usage: render.mjs <url> <allowed-domains-json> <allow-private-json>");
  process.exit(3);
}

const allowedDomains = JSON.parse(domainsJson);
const allowPrivate = JSON.parse(allowPrivateJson ?? "false") === true;
const maximumHtml = 16 * 1024 * 1024;

function privateAddress(address) {
  if (net.isIPv4(address)) {
    const parts = address.split(".").map(Number);
    return parts[0] === 0 || parts[0] === 10 || parts[0] === 127 ||
      (parts[0] === 169 && parts[1] === 254) ||
      (parts[0] === 172 && parts[1] >= 16 && parts[1] <= 31) ||
      (parts[0] === 192 && parts[1] === 168) || parts[0] >= 224;
  }
  if (net.isIPv6(address)) {
    const normalized = address.toLowerCase();
    return normalized === "::" || normalized === "::1" || normalized.startsWith("fc") ||
      normalized.startsWith("fd") || normalized.startsWith("fe8") ||
      normalized.startsWith("fe9") || normalized.startsWith("fea") ||
      normalized.startsWith("feb") || normalized.startsWith("ff");
  }
  return true;
}

async function validateUrl(value) {
  const parsed = new URL(value);
  if (parsed.protocol !== "http:" && parsed.protocol !== "https:") throw new Error("renderer only allows HTTP(S)");
  if (!allowedDomains.includes(parsed.hostname)) throw new Error(`renderer blocked outside domain: ${parsed.hostname}`);
  if (!allowPrivate) {
    const addresses = await dns.lookup(parsed.hostname, { all: true, verbatim: true });
    if (!addresses.length || addresses.some(({ address }) => privateAddress(address)))
      throw new Error(`renderer blocked private address: ${parsed.hostname}`);
  }
}

let browser;
try {
  await validateUrl(target);
  browser = await chromium.launch({ headless: true });
  const context = await browser.newContext({ serviceWorkers: "block" });
  const page = await context.newPage();
  await page.route("**/*", async (route) => {
    try {
      await validateUrl(route.request().url());
      await route.continue();
    } catch {
      await route.abort("blockedbyclient");
    }
  });
  const response = await page.goto(target, { waitUntil: "networkidle", timeout: 30000 });
  if (!response || !response.ok()) throw new Error(`renderer navigation failed: ${response?.status() ?? "no response"}`);
  const html = await page.content();
  if (Buffer.byteLength(html) > maximumHtml) throw new Error("rendered HTML exceeds 16 MiB limit");
  process.stdout.write(html);
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 4;
} finally {
  await browser?.close();
}
