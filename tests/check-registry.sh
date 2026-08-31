#!/bin/sh
set -eu

HHY_BIN=${1:-build/hhy}
fixture=$(mktemp -d)
home=$(mktemp -d)
trap 'rm -rf "$fixture" "$home"' EXIT INT TERM

python3 tests/create_registry_fixture.py "$fixture"

plan=$(HHY_EXTENSION_HOME="$home" "$HHY_BIN" install --dry-run \
    --registry "$fixture" --trust-root "$fixture/root.json" official/sample)
case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) native_target=darwin-arm64 ;;
    Linux-x86_64) native_target=linux-x86_64 ;;
    Linux-aarch64) native_target=linux-arm64 ;;
    MINGW*-x86_64|MSYS*-x86_64) native_target=windows-x86_64 ;;
    *) native_target=unsupported ;;
esac
printf '%s' "$plan" | grep -F "1. official/html 0.1.0 [$native_target] -> html" >/dev/null
printf '%s' "$plan" | grep -F "2. official/sample 0.1.0 [$native_target] -> sample" >/dev/null
[ ! -e "$home/html" ] && [ ! -e "$home/sample" ]

HHY_EXTENSION_HOME="$home" "$HHY_BIN" install --yes \
    --registry "$fixture" --trust-root "$fixture/root.json" official/sample >/dev/null
HHY_EXTENSION_HOME="$home" "$HHY_BIN" list | grep -F 'html 0.1.0' >/dev/null
HHY_EXTENSION_HOME="$home" "$HHY_BIN" list | grep -F 'sample 0.1.0' >/dev/null

# A late conflict rolls back dependencies added by this transaction and preserves the old package.
rollback_home=$(mktemp -d)
HHY_EXTENSION_HOME="$rollback_home" "$HHY_BIN" install --yes extensions/sample >/dev/null
set +e
HHY_EXTENSION_HOME="$rollback_home" "$HHY_BIN" install --yes \
    --registry "$fixture" --trust-root "$fixture/root.json" official/sample >/dev/null 2>&1
rollback_status=$?
set -e
[ "$rollback_status" -ne 0 ]
[ -d "$rollback_home/sample" ]
[ ! -e "$rollback_home/html" ]
rm -rf "$rollback_home"

# A signed descriptor cannot make a modified payload acceptable.
payload_home=$(mktemp -d)
printf '\n' >> "$fixture/packages/official/sample/0.1.0/$native_target/hhy.toml"
set +e
HHY_EXTENSION_HOME="$payload_home" "$HHY_BIN" install --yes \
    --registry "$fixture" --trust-root "$fixture/root.json" official/sample >/dev/null 2>&1
payload_status=$?
set -e
[ "$payload_status" -ne 0 ]
[ -z "$(find "$payload_home" -mindepth 1 -print -quit)" ]
rm -rf "$payload_home"
python3 tests/create_registry_fixture.py "$fixture.rebuilt"
rm -rf "$fixture"
mv "$fixture.rebuilt" "$fixture"

# Any index mutation must fail before touching the installation home.
tampered_home=$(mktemp -d)
python3 -c 'from pathlib import Path; p=Path(__import__("sys").argv[1]); p.write_text(p.read_text().replace("hhy-test", "hhy-tampered", 1))' "$fixture/index.json"
set +e
HHY_EXTENSION_HOME="$tampered_home" "$HHY_BIN" install --yes \
    --registry "$fixture" --trust-root "$fixture/root.json" official/sample >/dev/null 2>&1
status=$?
set -e
[ "$status" -ne 0 ]
[ -z "$(find "$tampered_home" -mindepth 1 -print -quit)" ]
rm -rf "$tampered_home"

# A valid signed index without the native target fails closed.
missing_target_fixture=$(mktemp -d)
missing_target_home=$(mktemp -d)
case "$native_target" in
    linux-x86_64) other_target=darwin-arm64 ;;
    *) other_target=linux-x86_64 ;;
esac
HHY_REGISTRY_FIXTURE_TARGETS="$other_target" python3 tests/create_registry_fixture.py "$missing_target_fixture"
set +e
HHY_EXTENSION_HOME="$missing_target_home" "$HHY_BIN" install --yes \
    --registry "$missing_target_fixture" --trust-root "$missing_target_fixture/root.json" official/sample >/dev/null 2>&1
missing_target_status=$?
set -e
[ "$missing_target_status" -ne 0 ]
[ -z "$(find "$missing_target_home" -mindepth 1 -print -quit)" ]
rm -rf "$missing_target_fixture" "$missing_target_home"

# The same identity/version may have many targets, but one target coordinate may occur only once.
duplicate_fixture=$(mktemp -d)
duplicate_home=$(mktemp -d)
HHY_REGISTRY_FIXTURE_TARGETS="$native_target,$native_target" python3 tests/create_registry_fixture.py "$duplicate_fixture"
set +e
HHY_EXTENSION_HOME="$duplicate_home" "$HHY_BIN" install --yes \
    --registry "$duplicate_fixture" --trust-root "$duplicate_fixture/root.json" official/sample >/dev/null 2>&1
duplicate_status=$?
set -e
[ "$duplicate_status" -ne 0 ]
[ -z "$(find "$duplicate_home" -mindepth 1 -print -quit)" ]
rm -rf "$duplicate_fixture" "$duplicate_home"

echo 'signed Registry tests passed'
