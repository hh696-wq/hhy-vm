#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const sourcePath = path.join(root, "syntax", "hhy-syntax.json");
const source = JSON.parse(fs.readFileSync(sourcePath, "utf8"));

const escapeRegex = (value) => value.replace(/[\\^$.*+?()[\]{}|]/g, "\\$&");
const alternatives = (values) => [...values].sort((a, b) => b.length - a.length).map(escapeRegex).join("|");
const words = (values) => `\\b(?:${alternatives(values)})\\b`;
const generatedHeader = "Generated from editors/syntax/hhy-syntax.json. Do not edit by hand.";

const byteUnits = alternatives(source.literals.byteUnits);
const durationUnits = alternatives(source.literals.durationUnits);
const regexFlags = source.literals.regexFlags.join("");
// Keep editor highlighting deliberately linear. Nested repetition for complete
// character-class parsing can trigger pathological backtracking in Sublime's
// incremental lexer (and has crashed Sublime Text 4200 during catalogue scans).
// HHY's real Lexer remains the authority; false negatives in complex Regex
// literals are preferable to destabilizing the editor.
const regexBody = String.raw`/(?:\\.|[^/\\\n])+/[${regexFlags}]*`;
const regexPrefix = String.raw`(^|\b(?:let|return|throw|attempt|in|and|or|not)\s+|[=(,:\[!&|?{};>]\s*)(${regexBody})`;
const decimal = String.raw`(?:\d(?:[\d_]*\d)?|\d)`;

const patterns = {
  shebang: { match: "^#!.*$", name: "comment.line.shebang.hhy" },
  comment: { match: "#.*$", name: "comment.line.number-sign.hhy" },
  string: {
    begin: "\"",
    beginCaptures: { "0": { name: "punctuation.definition.string.begin.hhy" } },
    end: "\"|$",
    endCaptures: { "0": { name: "punctuation.definition.string.end.hhy" } },
    name: "string.quoted.double.hhy",
    patterns: [
      { match: "\\\\[nrt\\\"\\\\bf0]", name: "constant.character.escape.hhy" },
      { match: "\\\\.", name: "invalid.illegal.escape.hhy" }
    ]
  },
  regex: {
    match: regexPrefix,
    captures: {
      "2": { name: "string.regexp.hhy" }
    }
  },
  functionDeclaration: {
    match: "\\b(fn)\\s+([A-Za-z_][A-Za-z0-9_]*)",
    captures: {
      "1": { name: "storage.type.function.hhy" },
      "2": { name: "entity.name.function.hhy" }
    }
  },
  module: { match: words(["import", "export", "from", "as"]), name: "keyword.control.import.hhy" },
  declaration: { match: words(source.keywords.declaration.filter((word) => !["fn", "import", "export", "from", "as"].includes(word))), name: "storage.modifier.hhy" },
  control: { match: words(source.keywords.control), name: "keyword.control.hhy" },
  wordOperator: { match: words(source.keywords.operator), name: "keyword.operator.logical.hhy" },
  constants: { match: words(source.keywords.constants), name: "constant.language.hhy" },
  bytes: { match: `\\b${decimal}(?:\\.${decimal})?(?:[eE][+-]?${decimal})?(?:${byteUnits})\\b`, name: "constant.numeric.bytes.hhy" },
  duration: { match: `\\b${decimal}(?:\\.${decimal})?(?:[eE][+-]?${decimal})?(?:${durationUnits})\\b`, name: "constant.numeric.duration.hhy" },
  percent: { match: `\\b${decimal}(?:\\.${decimal})?(?:[eE][+-]?${decimal})?%`, name: "constant.numeric.percentage.hhy" },
  hexadecimal: { match: "\\b0[xX][0-9A-Fa-f_]+\\b", name: "constant.numeric.integer.hexadecimal.hhy" },
  binary: { match: "\\b0[bB][01_]+\\b", name: "constant.numeric.integer.binary.hhy" },
  float: { match: `\\b(?:${decimal}\\.${decimal}|${decimal}[eE][+-]?${decimal})\\b`, name: "constant.numeric.float.hhy" },
  integer: { match: `\\b${decimal}\\b`, name: "constant.numeric.integer.hhy" },
  builtins: { match: words(source.builtins), name: "support.function.builtin.hhy" },
  functionCall: { match: "\\b([A-Za-z_][A-Za-z0-9_]*)(?=\\s*\\()", captures: { "1": { name: "entity.name.function.call.hhy" } } },
  member: { match: "(?<=\\.)[A-Za-z_][A-Za-z0-9_]*", name: "variable.other.member.hhy" },
  operator: { match: "\\|>|\\?\\?|->|\\.\\.|==|!=|<=|>=|[+\\-*\\/%=<>]", name: "keyword.operator.hhy" }
};

const includeOrder = [
  "shebang", "comment", "string", "regex", "functionDeclaration", "module",
  "declaration", "control", "wordOperator", "constants", "bytes", "duration",
  "percent", "hexadecimal", "binary", "float", "integer", "builtins",
  "functionCall", "member", "operator"
];

// Sublime Text 4200 on arm64 crashed and spawned runaway catalogue crawlers
// when indexing the richer grammar. Keep this profile free of captures,
// lookarounds, nested repetition and Regex-literal parsing. Numeric rules below
// are deliberately flat: each repeated section consumes a concrete character.
// The VS Code grammar remains fully featured.
const sublimePatterns = {
  shebang: patterns.shebang,
  comment: patterns.comment,
  string: patterns.string,
  functionKeyword: { match: "\\bfn\\b", name: "storage.type.function.hhy" },
  module: patterns.module,
  declaration: patterns.declaration,
  control: patterns.control,
  wordOperator: patterns.wordOperator,
  constants: patterns.constants,
  bytes: { match: `\\b[0-9][0-9_]*(?:\\.[0-9][0-9_]*)?(?:${byteUnits})\\b`, name: "constant.numeric.bytes.hhy" },
  duration: { match: `\\b[0-9][0-9_]*(?:\\.[0-9][0-9_]*)?(?:${durationUnits})\\b`, name: "constant.numeric.duration.hhy" },
  percent: { match: "\\b[0-9][0-9_]*(?:\\.[0-9][0-9_]*)?%", name: "constant.numeric.percentage.hhy" },
  hexadecimal: patterns.hexadecimal,
  binary: patterns.binary,
  float: { match: "\\b[0-9][0-9_]*\\.[0-9][0-9_]*\\b", name: "constant.numeric.float.hhy" },
  integer: { match: "\\b[0-9][0-9_]*\\b", name: "constant.numeric.integer.hhy" },
  builtins: patterns.builtins,
  operator: patterns.operator
};
const sublimeIncludeOrder = Object.keys(sublimePatterns);

const grammar = {
  $schema: "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
  name: source.language.name,
  scopeName: source.language.scopeName,
  fileTypes: source.language.extensions.map((extension) => extension.slice(1)),
  firstLineMatch: source.language.firstLine,
  patterns: includeOrder.map((name) => ({ include: `#${name}` })),
  repository: Object.fromEntries(Object.entries(patterns).map(([name, pattern]) => [name, { patterns: [pattern] }])),
  metadata: { generated: generatedHeader }
};

const yamlQuote = (value) => `'${value.replaceAll("'", "''")}'`;
const scopeFor = (pattern) => pattern.name ?? pattern.captures?.["1"]?.name;
const sublimeRules = sublimeIncludeOrder.flatMap((name) => {
  const pattern = sublimePatterns[name];
  if (pattern.begin) {
    return [
      `    - match: ${yamlQuote(pattern.begin)}`,
      `      scope: punctuation.definition.string.begin.hhy`,
      `      push: double-quoted-string`
    ].join("\n");
  }
  const lines = [`    - match: ${yamlQuote(pattern.match)}`];
  if (pattern.captures) {
    lines.push("      captures:");
    for (const [capture, definition] of Object.entries(pattern.captures))
      lines.push(`        ${capture}: ${definition.name}`);
  } else {
    lines.push(`      scope: ${scopeFor(pattern)}`);
  }
  return lines.join("\n");
});

const sublime = `%YAML 1.2
---
# ${generatedHeader}
name: HHY
file_extensions:
  - hhy
first_line_match: ${yamlQuote(source.language.firstLine)}
scope: source.hhy
contexts:
  main:
${sublimeRules.join("\n")}

  double-quoted-string:
    - meta_scope: string.quoted.double.hhy
    - match: ${yamlQuote("\\\\[nrt\\\"\\\\bf0]")}
      scope: constant.character.escape.hhy
    - match: ${yamlQuote("\\\\.")}
      scope: invalid.illegal.escape.hhy
    - match: ${yamlQuote("\"")}
      scope: punctuation.definition.string.end.hhy
      pop: true
`;

const outputs = new Map([
  [path.join(root, "vscode", "syntaxes", "hhy.tmLanguage.json"), `${JSON.stringify(grammar, null, 2)}\n`],
  [path.join(root, "sublime", "HHY.sublime-syntax"), sublime]
]);

const check = process.argv.includes("--check");
let stale = false;
for (const [output, contents] of outputs) {
  if (check) {
    if (!fs.existsSync(output) || fs.readFileSync(output, "utf8") !== contents) {
      console.error(`stale generated file: ${path.relative(root, output)}`);
      stale = true;
    }
    continue;
  }
  fs.mkdirSync(path.dirname(output), { recursive: true });
  fs.writeFileSync(output, contents);
  console.log(`generated ${path.relative(root, output)}`);
}
if (stale) process.exit(1);
