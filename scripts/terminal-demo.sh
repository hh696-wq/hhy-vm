#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
HHY_BIN="${HHY_BIN:-${ROOT}/build/hhy}"
DELAY="${HHY_DEMO_DELAY:-4}"

pause() { [ "$DELAY" = "0" ] || sleep "$DELAY"; }
run() { printf '\n\033[1;36m$ %s\033[0m\n' "$*"; pause; "$@"; pause; }

[ -x "$HHY_BIN" ] || { printf 'Build HHY first: make\n' >&2; exit 1; }

printf '\033[1;34mHHY — Pipe Everything in 75 seconds\033[0m\n'
run "$HHY_BIN" --version
run "$HHY_BIN" check "$ROOT/examples/07-language-basics.hhy"
run "$HHY_BIN" run "$ROOT/examples/07-language-basics.hhy"
run "$HHY_BIN" run --dry-run "$ROOT/examples/06-backup-large-files.hhy" /tmp /tmp/hhy-demo-backup

printf '\n\033[1;32mNow the real challenge: a bounded, recursive site graph audit.\033[0m\n'
pause
run env HHY_BIN="$HHY_BIN" sh "$ROOT/practical-projects/sitegraph-auditor/self-test.sh"

printf '\n\033[1;32mOne language: check, plan, run, and safely crawl.\033[0m\n'
printf 'Next: https://hhylang.dev/zh/learn/quick-start\n'
