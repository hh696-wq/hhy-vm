#!/bin/sh
set -eu

root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
version="$(cat "$root/VERSION")"

sh -n "$root/install.sh"
sh -n "$root/scripts/terminal-demo.sh"
cmp "$root/install.sh" "$root/website/public/install.sh"
grep -F "hhyVersion = \"$version\"" "$root/website/src/lib/release.ts" >/dev/null
# Formula must remain installable while the next release is being built. Its
# version is advanced only after the published archive digest is available.
grep -E 'releases/download/v[0-9]+\.[0-9]+\.[0-9]+/hhy-[0-9]+\.[0-9]+\.[0-9]+-darwin-arm64\.tar\.gz' \
    "$root/Formula/hhy.rb" >/dev/null
grep -E 'sha256 "[0-9a-f]{64}"' "$root/Formula/hhy.rb" >/dev/null
grep -F "curl -fsSL https://hhylang.dev/install.sh | sh" "$root/README.md" >/dev/null

echo "promotion assets are synchronized for HHY $version"
