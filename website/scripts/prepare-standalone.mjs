import { cpSync, existsSync, mkdirSync } from "node:fs";
import { join } from "node:path";

const root = process.cwd();
const standalone = join(root, ".next", "standalone");
const staticSource = join(root, ".next", "static");
const publicSource = join(root, "public");

if (!existsSync(join(standalone, "server.js"))) {
  throw new Error(
    "Missing .next/standalone/server.js. Ensure next.config.ts uses output: 'standalone'.",
  );
}

mkdirSync(join(standalone, ".next"), { recursive: true });
cpSync(staticSource, join(standalone, ".next", "static"), {
  recursive: true,
  force: true,
});

if (existsSync(publicSource)) {
  cpSync(publicSource, join(standalone, "public"), {
    recursive: true,
    force: true,
  });
}

console.log("Prepared .next/standalone with static and public assets.");
