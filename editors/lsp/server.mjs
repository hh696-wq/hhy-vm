#!/usr/bin/env node

import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, "../..");
const hhy = process.env.HHY_BIN || path.join(repo, "build", "hhy");
const documents = new Map();
let input = Buffer.alloc(0);

let contracts = [];
const contractRun = spawnSync(hhy, ["contracts", "--format", "json"], { encoding: "utf8" });
try { contracts = JSON.parse(contractRun.stdout).contracts || []; }
catch {
  let names = [];
  try {
    const source = fs.readFileSync(path.join(repo, "src/contracts.c"), "utf8");
    names = [...source.matchAll(/C\("([A-Za-z_][A-Za-z0-9_.]*)"/g)].map((match) => match[1]);
  } catch {
    const grammar = fs.readFileSync(path.join(here, "../syntaxes/hhy.tmLanguage.json"), "utf8");
    names = [...grammar.matchAll(/[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)?/g)]
      .map((match) => match[0]);
  }
  contracts = [...new Set(names)].map((name) => ({ name }));
}
contracts.sort((left, right) => left.name.localeCompare(right.name));
const contractsByName = new Map(contracts.map((contract) => [contract.name, contract]));

function send(payload) {
  const body = JSON.stringify(payload);
  process.stdout.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`);
}

function response(id, result) {
  send({ jsonrpc: "2.0", id, result });
}

function uriPath(uri) {
  if (!uri.startsWith("file://")) return null;
  return decodeURIComponent(new URL(uri).pathname);
}

function wordAt(text, position) {
  const lines = text.split(/\r?\n/);
  const line = lines[position.line] || "";
  let start = Math.min(position.character, line.length);
  let end = start;
  while (start > 0 && /[A-Za-z0-9_.]/.test(line[start - 1])) start--;
  while (end < line.length && /[A-Za-z0-9_.]/.test(line[end])) end++;
  return line.slice(start, end);
}

function diagnosticPath(document) {
  const original = uriPath(document.uri);
  const directories = original ? [path.dirname(original), os.tmpdir()] : [os.tmpdir()];
  let failure;
  for (const directory of [...new Set(directories)]) {
    const temporary = path.join(directory, `.hhy-lsp-${process.pid}-${Date.now()}.hhy`);
    try {
      fs.writeFileSync(temporary, document.text, "utf8");
      return temporary;
    } catch (error) {
      failure = error;
    }
  }
  throw failure;
}

function publishDiagnostics(document) {
  let temporary;
  try {
    temporary = diagnosticPath(document);
    const run = spawnSync(hhy, ["check", "--format", "json", temporary], {
      encoding: "utf8",
      timeout: 10000,
    });
    const report = JSON.parse(run.stdout || "{}");
    const diagnostics = (report.diagnostics || []).map((item) => ({
      range: { start: item.start, end: item.end },
      severity: item.severity === "warning" ? 2 : 1,
      code: item.code,
      source: "hhy",
      message: item.message,
    }));
    send({ jsonrpc: "2.0", method: "textDocument/publishDiagnostics",
      params: { uri: document.uri, version: document.version, diagnostics } });
  } catch (error) {
    send({ jsonrpc: "2.0", method: "window/logMessage",
      params: { type: 1, message: `HHY diagnostics failed: ${error.message}` } });
  } finally {
    if (temporary) fs.rmSync(temporary, { force: true });
  }
}

function formatDocument(document) {
  let temporary;
  try {
    temporary = diagnosticPath(document);
    const run = spawnSync(hhy, ["fmt", temporary], { encoding: "utf8", timeout: 10000 });
    if (run.status !== 0) return [];
    const formatted = fs.readFileSync(temporary, "utf8");
    if (formatted === document.text) return [];
    return [{ range: { start: { line: 0, character: 0 },
      end: { line: document.text.split(/\r?\n/).length, character: 0 } }, newText: formatted }];
  } finally {
    if (temporary) fs.rmSync(temporary, { force: true });
  }
}

function definition(document, word) {
  if (!word) return null;
  const escaped = word.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const pattern = new RegExp(`\\b(?:let(?:\\s+mut)?|fn)\\s+(${escaped})\\b`);
  const lines = document.text.split(/\r?\n/);
  for (let line = 0; line < lines.length; line++) {
    const match = pattern.exec(lines[line]);
    if (match) {
      const character = match.index + match[0].lastIndexOf(word);
      return { uri: document.uri, range: { start: { line, character },
        end: { line, character: character + word.length } } };
    }
  }
  return null;
}

function handle(message) {
  const { id, method, params = {} } = message;
  if (method === "initialize") {
    response(id, { serverInfo: { name: "hhy-lsp", version: "1" }, capabilities: {
      textDocumentSync: 1, definitionProvider: true, hoverProvider: true,
      completionProvider: { triggerCharacters: ["."] }, documentFormattingProvider: true,
    } });
  } else if (method === "shutdown") response(id, null);
  else if (method === "exit") process.exit(0);
  else if (method === "textDocument/didOpen") {
    const document = params.textDocument; documents.set(document.uri, document); publishDiagnostics(document);
  } else if (method === "textDocument/didChange") {
    const current = documents.get(params.textDocument.uri);
    if (current && params.contentChanges?.[0]) {
      current.text = params.contentChanges[0].text;
      current.version = params.textDocument.version;
      publishDiagnostics(current);
    }
  } else if (method === "textDocument/didClose") {
    documents.delete(params.textDocument.uri);
    send({ jsonrpc: "2.0", method: "textDocument/publishDiagnostics",
      params: { uri: params.textDocument.uri, diagnostics: [] } });
  } else if (method === "textDocument/completion") {
    response(id, contracts.map((contract) => ({ label: contract.name, kind: 3,
      detail: contract.input ? `${contract.input} -> ${contract.output}` : "HHY callable contract" })));
  } else if (method === "textDocument/hover") {
    const document = documents.get(params.textDocument.uri);
    const word = document ? wordAt(document.text, params.position) : "";
    const contract = contractsByName.get(word);
    response(id, contract ? { contents: { kind: "markdown", value: contract.input
      ? `\`${word}(${contract.input}) -> ${contract.output}\`\n\nEffect: **${contract.effect}** · Threading: **${contract.threading}** · Lazy: **${contract.lazy}** · Cancellable: **${contract.cancellable}**`
      : `\`${word}\`\n\nHHY callable registered in the Contract Registry.` } } : null);
  } else if (method === "textDocument/definition") {
    const document = documents.get(params.textDocument.uri);
    response(id, document ? definition(document, wordAt(document.text, params.position)) : null);
  } else if (method === "textDocument/formatting") {
    const document = documents.get(params.textDocument.uri);
    response(id, document ? formatDocument(document) : []);
  } else if (id !== undefined) response(id, null);
}

process.stdin.on("data", (chunk) => {
  input = Buffer.concat([input, chunk]);
  while (true) {
    const headerEnd = input.indexOf("\r\n\r\n");
    if (headerEnd < 0) break;
    const header = input.subarray(0, headerEnd).toString("ascii");
    const match = /Content-Length:\s*(\d+)/i.exec(header);
    if (!match) { input = input.subarray(headerEnd + 4); continue; }
    const length = Number(match[1]);
    if (input.length < headerEnd + 4 + length) break;
    const body = input.subarray(headerEnd + 4, headerEnd + 4 + length).toString("utf8");
    input = input.subarray(headerEnd + 4 + length);
    try { handle(JSON.parse(body)); }
    catch (error) { send({ jsonrpc: "2.0", method: "window/logMessage",
      params: { type: 1, message: `HHY LSP request failed: ${error.message}` } }); }
  }
});
