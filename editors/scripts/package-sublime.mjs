#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const source = path.join(root, "sublime");
const outputDirectory = path.join(root, "dist");
const output = path.join(outputDirectory, "HHY-0.1.0.sublime-package");

fs.mkdirSync(outputDirectory, { recursive: true });
fs.rmSync(output, { force: true });
const files = fs.readdirSync(source).filter((name) => !name.startsWith(".")).sort();
execFileSync("zip", ["-q", "-X", output, ...files], { cwd: source });
console.log(`packaged ${path.relative(root, output)}`);
