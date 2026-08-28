#!/bin/sh
set -eu

root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
version="$(cat "$root/VERSION")"

sh -n "$root/install.sh"
sh -n "$root/scripts/terminal-demo.sh"
cmp "$root/install.sh" "$root/website/public/install.sh"
grep -F "hhyVersion = \"$version\"" "$root/website/src/lib/release.ts" >/dev/null
grep -F "releases/download/v$version/hhy-$version-darwin-arm64.tar.gz" "$root/Formula/hhy.rb" >/dev/null
grep -F "curl -fsSL https://hhylang.dev/install.sh | sh" "$root/README.md" >/dev/null

echo "promotion assets are synchronized for HHY $version"
