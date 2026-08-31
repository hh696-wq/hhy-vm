#!/usr/bin/env node

import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
fs.mkdirSync(path.join(root, "tests/output"), { recursive: true });
const child = spawn(process.execPath, [path.join(root, "editors/lsp/server.mjs")], {
  cwd: root, env: { ...process.env, HHY_BIN: path.join(root, "build/hhy") },
});
let buffer = Buffer.alloc(0);
const messages = [];
child.stdout.on("data", (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  while (true) {
    const end = buffer.indexOf("\r\n\r\n");
    if (end < 0) return;
    const header = buffer.subarray(0, end).toString("ascii");
    const length = Number(/Content-Length:\s*(\d+)/i.exec(header)?.[1]);
    if (buffer.length < end + 4 + length) return;
    messages.push(JSON.parse(buffer.subarray(end + 4, end + 4 + length).toString("utf8")));
    buffer = buffer.subarray(end + 4 + length);
  }
});
function send(payload) {
  const body = JSON.stringify(payload);
  child.stdin.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`);
}
async function waitFor(predicate) {
  for (let index = 0; index < 100; index++) {
    const found = messages.find(predicate);
    if (found) return found;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  throw new Error("timed out waiting for LSP response");
}

send({ jsonrpc: "2.0", id: 1, method: "initialize", params: {} });
const initialized = await waitFor((item) => item.id === 1);
assert.equal(initialized.result.serverInfo.name, "hhy-lsp");
assert.equal(initialized.result.capabilities.documentFormattingProvider, true);

const uri = pathToFileURL(path.join(root, "tests/output/lsp-fixture.hhy")).href;
send({ jsonrpc: "2.0", method: "textDocument/didOpen", params: { textDocument: {
  uri, languageId: "hhy", version: 1, text: "let value =\n",
} } });
const diagnostics = await waitFor((item) => item.method === "textDocument/publishDiagnostics");
assert.equal(diagnostics.params.diagnostics[0].source, "hhy");
assert.equal(diagnostics.params.diagnostics[0].code, "HHY_SYNTAX");

send({ jsonrpc: "2.0", id: 2, method: "textDocument/completion",
  params: { textDocument: { uri }, position: { line: 1, character: 2 } } });
const completion = await waitFor((item) => item.id === 2);
assert(completion.result.some((item) => item.label === "print"));

send({ jsonrpc: "2.0", method: "textDocument/didChange", params: {
  textDocument: { uri, version: 2 }, contentChanges: [{ text: "let value = 1   \nprint(value)\n" }],
} });
const clean = await waitFor((item) => item.method === "textDocument/publishDiagnostics" && item.params.version === 2);
assert.deepEqual(clean.params.diagnostics, []);

send({ jsonrpc: "2.0", id: 3, method: "textDocument/hover",
  params: { textDocument: { uri }, position: { line: 1, character: 2 } } });
const hover = await waitFor((item) => item.id === 3);
assert.match(hover.result.contents.value, /Effect:.*custom/);

send({ jsonrpc: "2.0", id: 4, method: "textDocument/definition",
  params: { textDocument: { uri }, position: { line: 1, character: 8 } } });
const definition = await waitFor((item) => item.id === 4);
assert.equal(definition.result.range.start.line, 0);

send({ jsonrpc: "2.0", id: 5, method: "textDocument/formatting",
  params: { textDocument: { uri }, options: { tabSize: 4, insertSpaces: true } } });
const formatting = await waitFor((item) => item.id === 5);
assert.match(formatting.result[0].newText, /^let value = 1$/m);

send({ jsonrpc: "2.0", id: 6, method: "shutdown", params: null });
await waitFor((item) => item.id === 6);
send({ jsonrpc: "2.0", method: "exit", params: null });
await new Promise((resolve) => child.once("exit", resolve));
console.log("HHY LSP protocol verification passed");
