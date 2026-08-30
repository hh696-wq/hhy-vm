#!/usr/bin/env node

import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const archive = path.join(root, "dist", "HHY-0.1.0.sublime-package");
const source = path.join(root, "sublime");
const expected = fs.readdirSync(source).filter((name) => !name.startsWith(".")).sort();

assert.ok(fs.existsSync(archive), "Sublime package is required; run npm run package:sublime first");
const entries = execFileSync("unzip", ["-Z1", archive], { encoding: "utf8" }).trim().split("\n").filter(Boolean).sort();
assert.deepEqual(entries, expected, "package must contain exactly the public Sublime files");
assert.ok(entries.every((entry) => path.basename(entry) === entry), "package entries must not escape the package root");

const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "hhy-sublime-package-"));
try {
  execFileSync("unzip", ["-qq", archive, "-d", temporary]);
  for (const entry of expected) {
    const packaged = fs.readFileSync(path.join(temporary, entry));
    const original = fs.readFileSync(path.join(source, entry));
    assert.deepEqual(packaged, original, `packaged ${entry} must match its source`);
  }
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}

console.log("HHY Sublime package verification passed");
