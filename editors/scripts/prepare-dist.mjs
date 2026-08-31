#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
fs.mkdirSync(path.join(root, "dist"), { recursive: true });
fs.mkdirSync(path.join(root, "vscode/lsp"), { recursive: true });
fs.copyFileSync(path.join(root, "lsp/server.mjs"), path.join(root, "vscode/lsp/server.mjs"));
execFileSync(path.join(root, "vscode/node_modules/.bin/esbuild"), [
  path.join(root, "vscode/src/extension.cjs"),
  "--bundle",
  "--minify",
  "--external:vscode",
  "--format=cjs",
  `--outfile=${path.join(root, "vscode/extension.cjs")}`,
], { stdio: "inherit" });
