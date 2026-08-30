#!/usr/bin/env node

import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const repo = path.resolve(root, "..");
const readJson = (relative) => JSON.parse(fs.readFileSync(path.join(root, relative), "utf8"));
const source = readJson("syntax/hhy-syntax.json");
const grammar = readJson("vscode/syntaxes/hhy.tmLanguage.json");
const extension = readJson("vscode/package.json");
const languageConfiguration = readJson("vscode/language-configuration.json");
const snippets = readJson("vscode/snippets/hhy.json");
const lexer = fs.readFileSync(path.join(repo, "src", "lexer.c"), "utf8");
const contracts = fs.readFileSync(path.join(repo, "src", "contracts.c"), "utf8");
const sublime = fs.readFileSync(path.join(root, "sublime", "HHY.sublime-syntax"), "utf8");
const sublimeSyntaxTest = fs.readFileSync(path.join(root, "tests", "syntax_test_hhy.hhy"), "utf8");

assert.equal(source.language.id, "hhy");
assert.equal(grammar.scopeName, "source.hhy");
assert.deepEqual(grammar.fileTypes, ["hhy"]);
assert.equal(extension.contributes.grammars[0].scopeName, grammar.scopeName);
assert.deepEqual(extension.contributes.languages[0].extensions, [".hhy"]);
assert.equal(languageConfiguration.comments.lineComment, "#");
assert.ok(Object.keys(snippets).length >= 5);
assert.match(sublime, /^%YAML 1\.2/);
assert.match(sublime, /scope: source\.hhy/);
assert.doesNotMatch(
  sublime,
  /\\\[\(\?:\\\\\.\|\[\^\\\]\\\\\]\)\*\\\]/,
  "Sublime Regex highlighting must not contain the nested character-class repetition that crashes Build 4200"
);
assert.doesNotMatch(sublime, /string\.regexp\.hhy/, "Sublime 4200 safety grammar must not enable Regex-literal highlighting");
assert.doesNotMatch(sublime, /\(\?<|\(\?=/, "Sublime 4200 safety grammar must not contain lookarounds");
assert.doesNotMatch(sublime, /captures:/, "Sublime 4200 safety grammar must not contain capture rules");
assert.match(sublime, /storage\.type\.function\.hhy/, "Sublime safety grammar must highlight fn");
assert.match(sublime, /constant\.numeric\.integer\.hhy/, "Sublime safety grammar must highlight simple integers");
for (const scope of [
  "constant.numeric.integer.hexadecimal.hhy",
  "constant.numeric.integer.binary.hhy",
  "constant.numeric.float.hhy",
  "constant.numeric.percentage.hhy",
  "constant.numeric.bytes.hhy",
  "constant.numeric.duration.hhy"
]) {
  assert.ok(sublime.includes(scope), `Sublime safety grammar must include ${scope}`);
  assert.ok(sublimeSyntaxTest.includes(scope), `Sublime syntax test must cover ${scope}`);
}
assert.match(sublimeSyntaxTest, /^# SYNTAX TEST "Packages\/HHY\/HHY\.sublime-syntax"$/m);

const lexerKeywords = [...lexer.matchAll(/\{"([a-z]+)", HHY_T_[A-Z_]+\}/g)].map((match) => match[1]);
const sourceKeywords = Object.values(source.keywords).flat().sort();
assert.deepEqual([...lexerKeywords].sort(), sourceKeywords, "syntax keyword source must match src/lexer.c");

for (const unit of [...source.literals.byteUnits, ...source.literals.durationUnits])
  assert.ok(lexer.includes(`"${unit}"`), `unit ${unit} must exist in lexer`);
for (const flag of source.literals.regexFlags)
  assert.match(lexer, new RegExp(`strchr\\(\"imsu\"`), `regex flag ${flag} must match lexer flags`);
for (const builtin of source.builtins)
  assert.ok(contracts.includes(`C("${builtin}"`), `builtin ${builtin} must exist in callable contracts`);

const repositoryText = JSON.stringify(grammar.repository);
for (const keyword of sourceKeywords)
  assert.ok(repositoryText.includes(keyword), `generated TextMate grammar must include ${keyword}`);
for (const operator of ["|>", "??", "->", ".."]) {
  assert.ok(source.operators.includes(operator));
}

const hhy = path.join(repo, "build", "hhy");
assert.ok(fs.existsSync(hhy), "build/hhy is required; run make first");
for (const fixture of fs.readdirSync(path.join(root, "fixtures")).filter((name) => name.endsWith(".hhy")).sort()) {
  execFileSync(hhy, ["check", path.join(root, "fixtures", fixture)], { stdio: "pipe" });
}

console.log("HHY editor support verification passed");
