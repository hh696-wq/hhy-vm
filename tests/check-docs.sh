#!/bin/sh
set -eu

HHY_BIN=${1:-build/hhy}
DOCUMENT=${2:-README.md}
TEMPORARY=$(mktemp -d "${TMPDIR:-/tmp}/hhy-docs.XXXXXX")
trap 'rm -rf "$TEMPORARY"' EXIT INT TERM

awk -v directory="$TEMPORARY" '
    BEGIN { active = 0; count = 0 }
    /^```hhy[[:space:]]*$/ {
        active = 1
        count++
        file = sprintf("%s/example-%03d.hhy", directory, count)
        next
    }
    /^```[[:space:]]*$/ && active {
        active = 0
        next
    }
    active { print > file }
    END { print count > (directory "/count") }
' "$DOCUMENT"

count=$(cat "$TEMPORARY/count")
[ "$count" -gt 0 ] || {
    echo "test failure: no executable HHY documentation blocks found" >&2
    exit 1
}

for source in "$TEMPORARY"/example-*.hhy; do
    "$HHY_BIN" check "$source" >/dev/null || {
        echo "test failure: complete documentation example does not pass syntax and semantic checks: $source" >&2
        exit 1
    }
done

echo "validated $count complete HHY documentation blocks with Parser and Checker"
