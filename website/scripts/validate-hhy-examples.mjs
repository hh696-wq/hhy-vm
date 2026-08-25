import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

const sourcePath = resolve("src/lib/docs.ts");
const hhyBinary = resolve("../build/hhy");
const source = readFileSync(sourcePath, "utf8");
const object = source.match(/const code = \{([\s\S]*?)\n\};/);

if (!object) {
  throw new Error("could not find the documentation code registry");
}

const examples = [...object[1].matchAll(/\n\s+(\w+): `([\s\S]*?)`(?=,?\n\s+\w+:|\n$)/g)]
  .filter((match) => match[1] !== "cli")
  .map((match) => ({ name: match[1], code: match[2] }));

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
  console.log(`validated ${examples.length} HHY website examples`);
} finally {
  rmSync(temporary, { recursive: true, force: true });
}
