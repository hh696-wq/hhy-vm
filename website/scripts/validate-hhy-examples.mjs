import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

const sourcePath = resolve("src/lib/docs.ts");
const hhyBinary = resolve("../build/hhy");
const source = readFileSync(sourcePath, "utf8");
const contractsSource = readFileSync(resolve("../src/contracts.c"), "utf8");
const object = source.match(/const code = \{([\s\S]*?)\n\};/);

if (!object) {
  throw new Error("could not find the documentation code registry");
}

const referenceBlocks = new Set([
  "cli",
  "syntaxOperators",
  "stdCore",
  "stdFlow",
  "stdText",
  "stdFiles",
  "stdProcess",
  "stdHttp"
]);

const codeBlocks = [...object[1].matchAll(/\n\s+(\w+): `([\s\S]*?)`(?=,?\n\s+\w+:|,?\s*$)/g)];
const examples = codeBlocks
  .filter((match) => !referenceBlocks.has(match[1]))
  .map((match) => ({ name: match[1], code: match[2] }));
const inlineExamples = [...source.matchAll(/\{ type: "code", language: "hhy", code: "((?:\\.|[^"\\])*)" \}/g)]
  .map((match, index) => ({ name: `inline-${index + 1}`, code: JSON.parse(`"${match[1]}"`) }));
examples.push(...inlineExamples);

const callableBlocks = new Set(["stdCore", "stdFlow", "stdText", "stdFiles", "stdProcess", "stdHttp"]);
const documentedCallables = codeBlocks
  .filter((match) => callableBlocks.has(match[1]))
  .flatMap((match) => [...match[2].matchAll(/^([a-z_]+(?:\.[a-z_]+)?)\(/gm)].map((item) => item[1]));
const runtimeCallables = [...contractsSource.matchAll(/\bC\("([^"]+)"/g)].map((match) => match[1]);
const missingCallables = runtimeCallables.filter((name) => !documentedCallables.includes(name));
const unknownCallables = documentedCallables.filter((name) => !runtimeCallables.includes(name));

if (missingCallables.length || unknownCallables.length || documentedCallables.length !== runtimeCallables.length) {
  throw new Error(`standard-library index differs from Runtime Registry; missing=[${missingCallables}], unknown=[${unknownCallables}]`);
}

if (examples.length < 10) {
  throw new Error(`expected at least 10 HHY examples, found ${examples.length}`);
}

const temporary = mkdtempSync(join(tmpdir(), "hhy-website-examples-"));

try {
  writeFileSync(join(temporary, "math.hhy"), "export fn add(a, b) { return a + b }\n");
  for (const example of examples) {
    const file = join(temporary, `${example.name}.hhy`);
    writeFileSync(file, `${example.code}\n`);
    const result = spawnSync(hhyBinary, ["check", file], { encoding: "utf8" });
    if (result.status !== 0) {
      process.stderr.write(result.stderr);
      throw new Error(`HHY documentation example failed: ${example.name}`);
    }
  }
  console.log(`validated ${examples.length} HHY website examples and ${documentedCallables.length} callable signatures`);
} finally {
  rmSync(temporary, { recursive: true, force: true });
}
