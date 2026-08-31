#!/bin/sh
set -eu

HHY_BIN=${1:-build/hhy}
extension_test_home=$(mktemp -d)
trap 'rm -rf "$extension_test_home"' EXIT INT TERM
HHY_EXTENSION_HOME="$extension_test_home" "$HHY_BIN" install --yes extensions/sample >/dev/null
HHY_EXTENSION_HOME="$extension_test_home" "$HHY_BIN" install --yes extensions/database >/dev/null
HHY_EXTENSION_HOME="$extension_test_home" "$HHY_BIN" install --yes extensions/html >/dev/null
export HHY_EXTENSION_HOME="$extension_test_home"

# Generated test artifacts are intentionally ignored by Git. A clean checkout
# must create the root before the first lexer/parser snapshot is written.
mkdir -p tests/output

sh tests/check-version.sh "$HHY_BIN"

fail() {
    echo "test failure: $1" >&2
    exit 1
}

process_snapshot_available=0
socket_bind_available=0
if command -v python3 >/dev/null 2>&1; then
    if python3 tests/capabilities.py process-snapshot --quiet; then
        process_snapshot_available=1
    fi
    if python3 tests/capabilities.py socket-bind --quiet; then
        socket_bind_available=1
    fi
fi

if grep -En 'Value \*[^;=]*= hhy_(alloc|realloc)|Value \*[^;]*hhy_realloc' src/runtime.c >/dev/null; then
    fail "managed Value buffer uses a native allocator"
fi

for source in examples/*.hhy tests/valid/*.hhy tests/acceptance/*.hhy tests/acceptance/lib/*.hhy; do
    "$HHY_BIN" check "$source" >/dev/null || fail "expected $source to parse"
done

for source in examples/*.hhy; do
    "$HHY_BIN" fmt --check "$source" >/dev/null ||
        fail "official example is not in canonical format: $source"
done

sh tests/check-docs.sh "$HHY_BIN" >/dev/null
sh tests/check-docs.sh "$HHY_BIN" README.md >/dev/null
if command -v python3 >/dev/null 2>&1; then
    python3 tests/check_contracts.py >/dev/null
fi

# Deterministic GC pressure runs in the Release suite. ASan uses a fake stack
# on some architectures, which is intentionally incompatible with using a
# conservative collector's forced collection as a root-reachability oracle.
if [ "${HHY_SKIP_GC_STRESS:-0}" != 1 ]; then
    for gc_case in \
        tests/valid/advanced-flow.hhy \
        tests/valid/json-flow.hhy \
        tests/valid/csv-flow.hhy \
        tests/valid/gc-distinct-dynamic.hhy \
        tests/valid/debounce.hhy; do
        gc_expected=$("$HHY_BIN" run "$gc_case")
        gc_actual=$(HHY_GC_STRESS=1 "$HHY_BIN" run "$gc_case")
        [ "$gc_actual" = "$gc_expected" ] ||
            fail "GC stress changed output for $gc_case"
    done
fi

for source in tests/invalid/*.hhy; do
    if "$HHY_BIN" check "$source" >/dev/null 2>&1; then
        fail "expected $source to be rejected"
    fi
done

for source in tests/invalid-check/*.hhy; do
    set +e
    "$HHY_BIN" check "$source" >/dev/null 2>&1
    check_status=$?
    set -e
    if [ "$check_status" -ne 2 ]; then
        fail "semantic checker did not reject $source with status 2"
    fi
done

set +e
qualified_module_error=$("$HHY_BIN" check tests/invalid-check/missing-qualified-module.hhy 2>&1)
qualified_module_status=$?
set -e
[ "$qualified_module_status" -eq 2 ] || fail "missing qualified module used the wrong exit status"
case "$qualified_module_error" in
    *"ModuleNotFoundError(HHY_MODULE_NOT_FOUND)"*) ;;
    *) fail "missing qualified module did not report the stable ModuleNotFoundError code" ;;
esac

"$HHY_BIN" tokens examples/07-language-basics.hhy > tests/output/language-basics.tokens.txt
cmp -s tests/output/language-basics.tokens.txt tests/fixtures/language-basics.tokens.txt ||
    fail "Lexer token snapshot changed"
"$HHY_BIN" ast examples/07-language-basics.hhy > tests/output/language-basics.ast.txt
cmp -s tests/output/language-basics.ast.txt tests/fixtures/language-basics.ast.txt ||
    fail "Parser AST snapshot changed"
set +e
recovery_output=$("$HHY_BIN" check tests/invalid/multiple-errors.hhy 2>&1)
recovery_status=$?
set -e
recovery_count=$(printf '%s\n' "$recovery_output" | grep -c "expected expression")
[ "$recovery_status" -eq 2 ] && [ "$recovery_count" -eq 2 ] ||
    fail "Parser did not recover and report both independent syntax errors"

version_output=$($HHY_BIN --version)
case "$version_output" in
    "hhy "*"© 2026 HHY Language contributors"*"Author: houhuiyang"*"License: Apache License 2.0"*"https://hhylang.dev/"*"huiyang.hou@qq.com") ;;
    *) fail "unexpected --version output" ;;
esac

extension_list=$($HHY_BIN list)
case "$extension_list" in
    *"sample 0.1.0"*"Author"*"HHY Official"*"Protocol"*"1"*"Permissions"*) ;;
    *) fail "sample extension was not listed: $extension_list" ;;
esac

extension_output=$($HHY_BIN run tests/valid/extension-sample.hhy)
case "$extension_output" in
    '{"name": "HHY", "values": [1, true, null]}
1') ;;
    *) fail "sample extension call returned unexpected output: $extension_output" ;;
esac

database_error_output=$($HHY_BIN run tests/valid/extension-database-error.hhy)
case "$database_error_output" in
    'false
DatabaseError
DATABASE_OPERATION_FAILED') ;;
    *) fail "database extension error did not cross the protocol safely: $database_error_output" ;;
esac

html_output=$($HHY_BIN run tests/valid/extension-html.hhy)
case "$html_output" in
    'HHY & Flow
/docs
true
[{"title": "HHY & Flow", "url": "/docs"}, {"title": "Second", "url": null}]') ;;
    *) fail "html extension returned unexpected output: $html_output" ;;
esac

url_output=$($HHY_BIN run tests/valid/url.hhy)
case "$url_output" in
    'https://example.com/guide?q=1
https
example.com
/guide
q=1
https://example.com/guide?q=1
false
false') ;;
    *) fail "URL normalization and resolution are incorrect: $url_output" ;;
esac

printf 'tamper' >> "$extension_test_home/sample/bin/hhy-sample"
if $HHY_BIN check tests/valid/extension-sample.hhy >/dev/null 2>&1; then
    fail "extension loader accepted a modified executable"
fi
$HHY_BIN remove sample >/dev/null
$HHY_BIN remove database >/dev/null
$HHY_BIN remove html >/dev/null
[ -z "$($HHY_BIN list)" ] || fail "removed extensions remain installed"
case "$extension_list" in
    *"database 0.2.0"*"Author"*"HHY Official"*"Protocol"*"1"*"Permissions"*) ;;
    *) fail "database extension was not listed: $extension_list" ;;
esac
case "$extension_list" in
    *"html 0.1.0"*"Author"*"HHY Official"*"Protocol"*"1"*"Permissions"*) ;;
    *) fail "html extension was not listed: $extension_list" ;;
esac

cp tests/fixtures/unformatted.hhy.txt tests/output/formatted.hhy
set +e
"$HHY_BIN" fmt --check tests/output/formatted.hhy >/dev/null 2>&1
fmt_check_before=$?
set -e
if [ "$fmt_check_before" -ne 2 ]; then
    fail "fmt --check accepted non-canonical source"
fi
"$HHY_BIN" fmt tests/output/formatted.hhy
cmp -s tests/output/formatted.hhy tests/fixtures/formatted.hhy.txt ||
    fail "fmt output did not match canonical fixture"
"$HHY_BIN" fmt --check tests/output/formatted.hhy >/dev/null ||
    fail "fmt --check rejected canonical source"
fmt_once=$(cksum tests/output/formatted.hhy)
"$HHY_BIN" fmt tests/output/formatted.hhy
fmt_twice=$(cksum tests/output/formatted.hhy)
[ "$fmt_once" = "$fmt_twice" ] || fail "fmt is not idempotent"

repl_output=$(printf 'let value = 40\nvalue + 2\nfn double(n) {\nreturn n * 2\n}\ndouble(value)\nvalue |>\ndouble\n:quit\n' | "$HHY_BIN" repl)
case "$repl_output" in
    "42
80
80") ;;
    *) fail "REPL did not preserve scope or execute multiline input: $repl_output" ;;
esac

run_output=$("$HHY_BIN" run tests/valid/basic.hhy)
case "$run_output" in
    "2
4
6") ;;
    *) fail "unexpected pipeline runtime output: $run_output" ;;
esac

profile_output=$("$HHY_BIN" profile tests/valid/basic.hhy 2>tests/output/basic.profile.txt)
[ "$profile_output" = "$run_output" ] || fail "profile changed script stdout"
grep -F 'HHY profile: tests/valid/basic.hhy' tests/output/basic.profile.txt >/dev/null ||
    fail "profile text report omitted its source"
grep -F 'CPU hotspots' tests/output/basic.profile.txt >/dev/null ||
    fail "profile text report omitted CPU hotspots"
grep -F 'Allocation hotspots' tests/output/basic.profile.txt >/dev/null ||
    fail "profile text report omitted allocation hotspots"
grep -F '<top-level>' tests/output/basic.profile.txt >/dev/null ||
    fail "profile did not account for top-level execution"

profile_json_output=$("$HHY_BIN" profile --heap --format json \
    --output tests/output/basic.profile.json tests/valid/basic.hhy)
[ "$profile_json_output" = "$run_output" ] || fail "JSON profile changed script stdout"
grep -F '"allocated_bytes"' tests/output/basic.profile.json >/dev/null ||
    fail "JSON profile omitted allocation data"
grep -F '"name": "<top-level>"' tests/output/basic.profile.json >/dev/null ||
    fail "JSON profile omitted top-level execution"
grep -F '"cpu_sample_period_us": 1000' tests/output/basic.profile.json >/dev/null ||
    fail "JSON profile omitted CPU sample period"
grep -F '"cpu_data_quality": "insufficient"' tests/output/basic.profile.json >/dev/null ||
    fail "JSON profile omitted low-sample quality marker"
grep -F '"warnings": [' tests/output/basic.profile.json >/dev/null ||
    fail "JSON profile omitted warnings"
if command -v python3 >/dev/null 2>&1; then
    python3 -m json.tool tests/output/basic.profile.json >/dev/null ||
        fail "profile JSON is invalid"
fi

set +e
"$HHY_BIN" profile --format yaml tests/valid/basic.hhy >/dev/null 2>&1
profile_option_status=$?
set -e
[ "$profile_option_status" -eq 3 ] || fail "invalid profile format used the wrong exit status"

bounded_event_output=$("$HHY_BIN" run tests/valid/bounded-event-stream.hhy)
[ "$bounded_event_output" = "2" ] || fail "take did not bound an event Stream"

int64_output=$("$HHY_BIN" run tests/valid/int64-exact.hhy)
case "$int64_output" in
    "true
1
0") ;;
    *) fail "Int64 arithmetic lost precision: $int64_output" ;;
esac

native_units_output=$("$HHY_BIN" run tests/valid/native-units.hhy)
case "$native_units_output" in
    "0b
10
255
1002
1001
true
true
true
true
[1000b, 2000000b]") ;;
    *) fail "native units or binary literal disambiguation is incorrect: $native_units_output" ;;
esac

gc_flow_output=$("$HHY_BIN" run tests/valid/gc-long-flow.hhy)
[ "$gc_flow_output" = "200000" ] || fail "long Flow lost state under GC pressure"

group_hash_scale_output=$("$HHY_BIN" run tests/valid/group-hash-scale.hhy)
[ "$group_hash_scale_output" = "20000" ] || fail "group_by hash scale test failed"

frame_escape_output=$("$HHY_BIN" run tests/valid/frame-slots-escape.hhy)
case "$frame_escape_output" in
    "11
12
101
[8, 9, 10]") ;;
    *) fail "slot frames did not preserve escaped closure or Stream state: $frame_escape_output" ;;
esac

set +e
unbounded_output=$("$HHY_BIN" run tests/invalid-runtime/unbounded-barrier.hhy 2>&1)
unbounded_status=$?
set -e
[ "$unbounded_status" -eq 1 ] || fail "unbounded barrier did not fail with runtime status 1"
case "$unbounded_output" in
    *"PlanError"*"apply take(n)"*) ;;
    *) fail "unbounded barrier did not produce a useful PlanError: $unbounded_output" ;;
esac

args_output=$("$HHY_BIN" run tests/valid/args.hhy hello hhy)
case "$args_output" in
    "HELLO
HHY") ;;
    *) fail "script arguments were not passed correctly: $args_output" ;;
esac

stdin_pick_output=$(printf 'flow\npipe\n' | HHY_TEST_REQUIRED=present "$HHY_BIN" run tests/valid/stdin-pick-env.hhy)
case "$stdin_pick_output" in
    '{"name": "HHY", "version": 1}
present
["FLOW", "PIPE"]') ;;
    *) fail "stdin_lines, pick or require_env failed: $stdin_pick_output" ;;
esac

shortcut_output=$("$HHY_BIN" tests/valid/args.hhy direct)
case "$shortcut_output" in
    'DIRECT') ;;
    *) fail "direct script CLI shortcut failed: $shortcut_output" ;;
esac

rm -f tests/output/dry-run.txt tests/output/dry-run-process.txt
dry_output=$("$HHY_BIN" run --dry-run tests/valid/dry-run.hhy 2>tests/output/dry-run.plan)
case "$dry_output" in
    "GET
https://example.invalid/path?<redacted>
2000000000ns
2
planned") ;;
    *) fail "dry-run produced unexpected output: $dry_output" ;;
esac
if [ -e tests/output/dry-run.txt ] || [ -e tests/output/dry-run-process.txt ]; then
    fail "dry-run allowed an external side effect"
fi
grep -Fx 'plan: write_text effect=filesystem lazy=false cancel=true' tests/output/dry-run.plan >/dev/null ||
    fail "Execution Plan omitted the filesystem contract"
grep -Fx 'plan: run effect=process lazy=false cancel=true' tests/output/dry-run.plan >/dev/null ||
    fail "Execution Plan omitted the process contract"
grep -Fx 'plan: send effect=network lazy=false cancel=true' tests/output/dry-run.plan >/dev/null ||
    fail "Execution Plan omitted the network contract"
grep -Fx 'plan: timeout effect=none lazy=false cancel=true' tests/output/dry-run.plan >/dev/null ||
    fail "Execution Plan omitted the timeout Flow contract"
grep -Fx 'plan: retry effect=none lazy=false cancel=true' tests/output/dry-run.plan >/dev/null ||
    fail "Execution Plan omitted the retry Flow contract"
if grep -E 'secret|token=' tests/output/dry-run.plan >/dev/null; then
    fail "Execution Plan leaked sensitive request data"
fi

dry_parallel_output=$("$HHY_BIN" run --dry-run --limit max_processes=1 \
    tests/valid/dry-run-parallel.hhy 2>tests/output/dry-run-parallel.plan)
[ "$dry_parallel_output" = '[2, 4, 6]' ] ||
    fail "dry-run parallel did not preserve lazy ordered values: $dry_parallel_output"
grep -Fx 'plan: parallel effect=process lazy=true cancel=true' \
    tests/output/dry-run-parallel.plan >/dev/null ||
    fail "Execution Plan omitted the parallel contract"

every_output=$("$HHY_BIN" run tests/valid/every.hhy)
case "$every_output" in
    "0
1
2") ;;
    *) fail "every Stream returned unexpected ticks: $every_output" ;;
esac

debounce_output=$("$HHY_BIN" run tests/valid/debounce.hhy)
case "$debounce_output" in
    "[1, 2, 3]") ;;
    *) fail "debounce did not coalesce repeated keys: $debounce_output" ;;
esac

parallel_output=$("$HHY_BIN" run tests/valid/parallel.hhy)
case "$parallel_output" in
    "[6, 2, 4, 8]") ;;
    *) fail "parallel did not preserve input order: $parallel_output" ;;
esac

set +e
parallel_error_output=$("$HHY_BIN" run tests/invalid-runtime/parallel-error.hhy 2>&1)
parallel_error_status=$?
parallel_send_output=$("$HHY_BIN" run tests/invalid-runtime/parallel-non-sendable.hhy 2>&1)
parallel_send_status=$?
set -e
if [ "$parallel_error_status" -ne 1 ]; then
    fail "parallel worker error returned status $parallel_error_status instead of 1"
fi

process_options_output=$("$HHY_BIN" run tests/valid/process-options.hhy)
case "$process_options_output" in
    "HHY_STDIN
flow-env") ;;
    *) fail "run options were not applied correctly: $process_options_output" ;;
esac

shell_output=$("$HHY_BIN" run tests/valid/shell.hhy)
[ "$shell_output" = "Pipe Everything." ] || fail "explicit shell API failed: $shell_output"

set +e
"$HHY_BIN" run tests/invalid-runtime/process-timeout.hhy >/dev/null 2>&1
process_timeout_status=$?
set -e
if [ "$process_timeout_status" -ne 5 ]; then
    fail "process timeout returned status $process_timeout_status instead of 5"
fi
case "$parallel_error_output" in
    *"parallel boom"*) ;;
    *) fail "parallel worker error did not propagate its value: $parallel_error_output" ;;
esac
if [ "$parallel_send_status" -ne 1 ]; then
    fail "parallel accepted a non-sendable worker result"
fi

if command -v python3 >/dev/null 2>&1; then
    python3 - "$HHY_BIN" <<'PY' || fail "parallel early close left workers running"
import subprocess
import sys

result = subprocess.run(
    [sys.argv[1], "run", "tests/valid/parallel-early-close.hhy"],
    capture_output=True,
    text=True,
    timeout=1.0,
)
assert result.returncode == 0, result.stderr
assert result.stdout.strip() == "[1]", result.stdout
PY
fi

"$HHY_BIN" run tests/valid/parallel-cancel.hhy >tests/output/parallel-cancel.log 2>&1 &
parallel_cancel_pid=$!
sleep 0.1
kill -INT "$parallel_cancel_pid"
set +e
wait "$parallel_cancel_pid"
parallel_cancel_status=$?
set -e
if [ "$parallel_cancel_status" -ne 5 ]; then
    fail "parallel Ctrl+C returned status $parallel_cancel_status instead of 5"
fi

"$HHY_BIN" run tests/valid/cancel.hhy >tests/output/cancel.log 2>&1 &
cancel_pid=$!
sleep 0.1
kill -INT "$cancel_pid"
set +e
wait "$cancel_pid"
cancel_status=$?
set -e
if [ "$cancel_status" -ne 5 ]; then
    fail "Ctrl+C cancellation returned exit status $cancel_status instead of 5"
fi

set +e
runtime_limit_output=$("$HHY_BIN" run --limit max_runtime=20ms tests/valid/cancel.hhy 2>&1)
runtime_limit_status=$?
cpu_loop_limit_output=$("$HHY_BIN" run --limit max_runtime=20ms tests/valid/cpu-loop-timeout.hhy 2>&1)
cpu_loop_limit_status=$?
open_limit_output=$("$HHY_BIN" run --limit max_open_files=1 tests/valid/process-options.hhy 2>&1)
open_limit_status=$?
process_limit_output=$("$HHY_BIN" run --limit max_processes=1 tests/valid/parallel.hhy 2>&1)
process_limit_status=$?
memory_limit_output=$("$HHY_BIN" run --limit max_memory=64kib tests/valid/gc-long-flow.hhy 2>&1)
memory_limit_status=$?
invalid_limit_output=$("$HHY_BIN" run --limit max_runtime=oops tests/valid/basic.hhy 2>&1)
invalid_limit_status=$?
set -e
[ "$runtime_limit_status" -eq 5 ] || fail "max_runtime returned status $runtime_limit_status"
case "$runtime_limit_output" in *"TimeoutError"*"max_runtime"*) ;; *) fail "max_runtime diagnostic is incorrect" ;; esac
[ "$cpu_loop_limit_status" -eq 5 ] || fail "CPU loop max_runtime returned status $cpu_loop_limit_status"
case "$cpu_loop_limit_output" in *"TimeoutError"*"max_runtime"*) ;; *) fail "CPU loop max_runtime diagnostic is incorrect" ;; esac
[ "$open_limit_status" -eq 1 ] || fail "max_open_files was not enforced"
case "$open_limit_output" in *"ResourceLimitError"*"max_open_files"*) ;; *) fail "max_open_files diagnostic is incorrect" ;; esac
[ "$process_limit_status" -eq 1 ] || fail "max_processes was not enforced"
case "$process_limit_output" in *"ResourceLimitError"*"max_processes"*) ;; *) fail "max_processes diagnostic is incorrect" ;; esac
[ "$memory_limit_status" -eq 1 ] || fail "max_memory returned status $memory_limit_status"
case "$memory_limit_output" in
    *"ResourceLimitError"*"max_memory"*) ;;
    *) fail "max_memory diagnostic is incorrect: $memory_limit_output" ;;
esac
[ "$invalid_limit_status" -eq 3 ] || fail "invalid --limit was not CLI usage error"

mkdir -p tests/output/memory-unwind
dd if=/dev/zero bs=1024 count=256 2>/dev/null | tr '\0' x >tests/output/memory-unwind/large.txt
printf 'preserved\n' >tests/output/memory-unwind/result.txt
set +e
memory_unwind_output=$("$HHY_BIN" run --limit max_memory=96kib \
    tests/valid/memory-atomic-unwind.hhy \
    tests/output/memory-unwind/large.txt tests/output/memory-unwind/result.txt 2>&1)
memory_unwind_status=$?
set -e
[ "$memory_unwind_status" -eq 1 ] || fail "memory unwind returned status $memory_unwind_status"
case "$memory_unwind_output" in
    *"ResourceLimitError"*"max_memory"*) ;;
    *) fail "memory unwind diagnostic is incorrect: $memory_unwind_output" ;;
esac
[ "$(cat tests/output/memory-unwind/result.txt)" = "preserved" ] ||
    fail "memory exhaustion damaged the previous atomic output"
find tests/output/memory-unwind -name '*.hhy-tmp-*' -print | grep . >/dev/null 2>&1 &&
    fail "memory exhaustion leaked an atomic temporary file"

mkdir -p tests/output/watch/nested
"$HHY_BIN" run tests/valid/watch-write.hhy tests/output/watch/nested/source.txt before
rm -f tests/output/watch-ready
"$HHY_BIN" run tests/valid/watch.hhy tests/output/watch tests/output/watch-ready >tests/output/watch.log 2>&1 &
watch_pid=$!
attempts=0
while [ ! -s tests/output/watch-ready ] && [ "$attempts" -lt 200 ]; do
    sleep 0.05
    attempts=$((attempts + 1))
done
[ -s tests/output/watch-ready ] || fail "watch did not signal readiness"
sleep 0.1
event_attempts=0
while kill -0 "$watch_pid" 2>/dev/null && [ "$event_attempts" -lt 50 ]; do
    "$HHY_BIN" run tests/valid/watch-write.hhy tests/output/watch/nested/source.txt \
        "after-$event_attempts"
    sleep 0.05
    event_attempts=$((event_attempts + 1))
done
attempts=0
while kill -0 "$watch_pid" 2>/dev/null && [ "$attempts" -lt 100 ]; do
    sleep 0.05
    attempts=$((attempts + 1))
done
if kill -0 "$watch_pid" 2>/dev/null; then
    kill -INT "$watch_pid" 2>/dev/null || true
    wait "$watch_pid" 2>/dev/null || true
    fail "watch did not emit a file event"
fi
wait "$watch_pid"
watch_output=$(sed -n '1p' tests/output/watch.log)
case "$watch_output" in
    '["modified", "FileEvent"]') ;;
    *) fail "watch emitted unexpected event: $watch_output" ;;
esac

mkdir -p tests/output/watch-rename
unlink tests/output/watch-rename/before.txt 2>/dev/null || true
unlink tests/output/watch-rename/after.txt 2>/dev/null || true
"$HHY_BIN" run tests/valid/watch-write.hhy tests/output/watch-rename/before.txt content
rm -f tests/output/watch-rename-ready
"$HHY_BIN" run tests/valid/watch-rename.hhy tests/output/watch-rename \
    tests/output/watch-rename-ready >tests/output/watch-rename.log 2>&1 &
rename_watch_pid=$!
attempts=0
while [ ! -s tests/output/watch-rename-ready ] && [ "$attempts" -lt 200 ]; do
    sleep 0.05
    attempts=$((attempts + 1))
done
[ -s tests/output/watch-rename-ready ] || fail "rename watch did not signal readiness"
sleep 0.1
event_attempts=0
while kill -0 "$rename_watch_pid" 2>/dev/null && [ "$event_attempts" -lt 50 ]; do
    if [ -e tests/output/watch-rename/before.txt ]; then
        mv tests/output/watch-rename/before.txt tests/output/watch-rename/after.txt
    else
        mv tests/output/watch-rename/after.txt tests/output/watch-rename/before.txt
    fi
    sleep 0.05
    event_attempts=$((event_attempts + 1))
done
attempts=0
while kill -0 "$rename_watch_pid" 2>/dev/null && [ "$attempts" -lt 100 ]; do
    sleep 0.05
    attempts=$((attempts + 1))
done
if kill -0 "$rename_watch_pid" 2>/dev/null; then
    kill -INT "$rename_watch_pid" 2>/dev/null || true
    wait "$rename_watch_pid" 2>/dev/null || true
    fail "watch did not emit a rename event"
fi
wait "$rename_watch_pid"
rename_watch_output=$(sed -n '1p' tests/output/watch-rename.log)
case "$rename_watch_output" in
    '["renamed", "before.txt", "after.txt"]'|'["renamed", "after.txt", "before.txt"]') ;;
    *) fail "watch rename normalization is incorrect: $rename_watch_output" ;;
esac

on_error_output=$("$HHY_BIN" run tests/valid/on-error.hhy 2>&1)
case "$on_error_output" in
    "fallback text
[\"fallback line\"]
successful scalar") ;;
    *) fail "on_error did not recover immediate and lazy errors: $on_error_output" ;;
esac

structured_error_output=$("$HHY_BIN" run tests/valid/structured-error.hhy)
case "$structured_error_output" in
    "false
IoError
HHY_IO
true
tests/valid/structured-error.hhy
read_text") ;;
    *) fail "structured Error fields are incorrect: $structured_error_output" ;;
esac

error_categories_output=$("$HHY_BIN" run tests/valid/error-categories.hhy)
case "$error_categories_output" in
    "IoError
HHY_IO
KeyError
HHY_MAP_KEY
TypeError
ValueError
IndexError
ValueError
ValueError") ;;
    *) fail "Error category contract is incorrect: $error_categories_output" ;;
esac

range_output=$("$HHY_BIN" run tests/valid/range.hhy)
case "$range_output" in
    "10
10
12
14
16") ;;
    *) fail "Range semantics are incorrect: $range_output" ;;
esac

datetime_output=$("$HHY_BIN" run tests/valid/datetime.hhy)
case "$datetime_output" in
    "true
2026-08-24T12:30:00Z
2000000000ns
DateTime") ;;
    *) fail "DateTime parse, timezone or arithmetic is incorrect: $datetime_output" ;;
esac

advanced_output=$("$HHY_BIN" run tests/valid/advanced-flow.hhy)
case "$advanced_output" in
    '2
1
[1, 2, 3, 4]
[{"team": "blue", "count": 2}, {"team": "red", "count": 1}]
true
true') ;;
    *) fail "advanced Flow operators returned unexpected content: $advanced_output" ;;
esac

group_hash_output=$("$HHY_BIN" run tests/valid/group-hash-semantics.hhy)
case "$group_hash_output" in
    '[["Int", 3], ["Int", 2], ["String", 1], ["Path", 1], ["Null", 2], ["Bool", 2]]') ;;
    *) fail "group_by hash semantics are incorrect: $group_hash_output" ;;
esac

collections_reduce_output=$("$HHY_BIN" run tests/valid/collections-reduce.hhy)
case "$collections_reduce_output" in
    "10
b
true
[\"first\", \"second\"]") ;;
    *) fail "collection or reduce contract is incorrect: $collections_reduce_output" ;;
esac

regex_output=$("$HHY_BIN" run tests/valid/regex.hhy)
case "$regex_output" in
    '["error disk", "WARN slow", "ERROR net"]
{"match": "418", "start": 5, "end": 8}
[{"match": "tom", "start": 5, "end": 8}, {"match": "42", "start": 13, "end": 15}]') ;;
    *) fail "Regex behavior is incorrect: $regex_output" ;;
esac

regex_flags_output=$("$HHY_BIN" run tests/valid/regex-flags-named.hhy)
case "$regex_flags_output" in
    "true
true
true
HHY
42") ;;
    *) fail "Regex flags or named captures are incorrect: $regex_flags_output" ;;
esac

csv_output=$("$HHY_BIN" run tests/valid/csv-flow.hhy)
case "$csv_output" in
    '["name,team,note", "Ada,blue,\"compiler, math\"", "Grace,navy,\"line one\nline two\""]') ;;
    *) fail "CSV streaming behavior is incorrect: $csv_output" ;;
esac

save_text_stream_output=$("$HHY_BIN" run tests/valid/save-text-stream.hhy)
[ -z "$save_text_stream_output" ] || fail "save_text unexpectedly printed output"
[ "$(cat tests/output/save-text-stream.txt)" = "HHY flows." ] ||
    fail "save_text did not concatenate Stream<String>"

rm -rf tests/output/nested
output_options=$("$HHY_BIN" run tests/valid/output-options.hhy)
case "$output_options" in
    "first
IoError
second
TypeError
second") ;;
    *) fail "atomic output options are incorrect: $output_options" ;;
esac
find tests/output/nested -name '*.hhy-tmp-*' -print | grep . >/dev/null 2>&1 &&
    fail "failed atomic output leaked a temporary file"

csv_options_output=$("$HHY_BIN" run tests/valid/csv-options.hhy)
case "$csv_options_output" in
    "[\"name;note\", \"Ada;'pipe;flow'\"]
[{\"name\": \"Ada\", \"note\": \"pipe;flow\"}]") ;;
    *) fail "CSV delimiter/quote options are incorrect: $csv_options_output" ;;
esac

mkdir -p tests/output/path-root/real
printf 'flow\n' >tests/output/path-root/real/a.txt
unlink tests/output/path-root/link 2>/dev/null || true
ln -s real tests/output/path-root/link
path_options_output=$("$HHY_BIN" run tests/valid/path-files-options.hhy tests/output/path-root)
case "$path_options_output" in
    "a/c
/a/c
.
runtime.c
.c
src
1
2") ;;
    *) fail "Path normalization or symlink traversal is incorrect: $path_options_output" ;;
esac

caught_output=$("$HHY_BIN" run tests/valid/caught-error.hhy 2>&1)
case "$caught_output" in
    "handled") ;;
    *) fail "caught error leaked a diagnostic: $caught_output" ;;
esac

lazy_output=$("$HHY_BIN" run tests/valid/lazy-stream.hhy)
case "$lazy_output" in
    "0
10
1") ;;
    *) fail "Stream is not lazy or take over-consumed: $lazy_output" ;;
esac

mkdir -p tests/output
file_output=$("$HHY_BIN" run tests/valid/file-flow.hhy)
case "$file_output" in
    "ERROR one
ERROR two") ;;
    *) fail "file-backed Flow returned unexpected content: $file_output" ;;
esac

json_output=$("$HHY_BIN" run tests/valid/json-flow.hhy)
case "$json_output" in
    '["Ada", "Grace"]') ;;
    *) fail "JSON Flow returned unexpected content: $json_output" ;;
esac

json_pretty_output=$("$HHY_BIN" run tests/valid/json-pretty.hhy)
case "$json_pretty_output" in
    '{
  "name": "HHY",
  "values": [
    1,
    true
  ]
}') ;;
    *) fail "pretty JSON output is incorrect: $json_pretty_output" ;;
esac

json_surrogate_output=$("$HHY_BIN" run tests/valid/json-unicode-surrogate.hhy)
case "$json_surrogate_output" in
    "😀
1
{\"emoji\": \"😀\"}") ;;
    *) fail "JSON UTF-16 surrogate pair decoding is incorrect: $json_surrogate_output" ;;
esac

string_nul_output=$("$HHY_BIN" run tests/valid/string-nul.hhy)
case "$string_nul_output" in
    "true
3
3
\"a\\u0000b\"
\"\\u0000\"
true
true
[\"a\", \"b\"]
\"a\\u0000b\"
\"a-b\"
\"A\\u0000B\"
true
true
7
{\"a\\u0000b\": 8}
{}
true
true
true
true
true
true
true") ;;
    *) fail "String embedded U+0000 semantics are incorrect: $string_nul_output" ;;
esac

process_output=$("$HHY_BIN" run tests/valid/process-flow.hhy)
case "$process_output" in
    "alpha
beta") ;;
    *) fail "process output Flow returned unexpected content: $process_output" ;;
esac

if [ "$process_snapshot_available" -eq 1 ]; then
  logical_record_output=$("$HHY_BIN" run tests/valid/logical-record-types.hhy)
  case "$logical_record_output" in
    "Result
Result
Result
KeyError
File
File
Directory
Directory
CommandResult
CommandResult
Process
Process") ;;
      *) fail "logical record types or parallel serialization are incorrect: $logical_record_output" ;;
  esac
else
  printf '%s\n' 'SKIP[HHY_CAP_PROCESS_SNAPSHOT]: host denies /bin/ps; process logical-record serialization is covered by CI'
fi

if [ "$process_snapshot_available" -eq 1 ]; then
  system_output=$("$HHY_BIN" run tests/valid/system.hhy)
  case "$system_output" in
    "String
Path
true
true") ;;
      *) fail "system namespaces or process Stream are incorrect: $system_output" ;;
  esac
else
  printf '%s\n' 'SKIP[HHY_CAP_PROCESS_SNAPSHOT]: host denies /bin/ps; system process Stream is covered by CI'
fi

module_output=$("$HHY_BIN" run tests/valid/modules.hhy)
case "$module_output" in
    "math
42
math
null
{\"present\": null}") ;;
    *) fail "module import or export behavior is incorrect: $module_output" ;;
esac

standard_module_output=$("$HHY_BIN" run tests/valid/standard-modules.hhy)
[ "$standard_module_output" = "Function
Function" ] || fail "standard module namespaces were not resolved from the registry"

stdlib_output=$("$HHY_BIN" run tests/valid/stdlib.hhy)
case "$stdlib_output" in
    '[2, 3]
HHY
{"version": 1}
a|b|c
43
true
flow') ;;
    *) fail "core standard library behavior is incorrect: $stdlib_output" ;;
esac

unicode_output=$("$HHY_BIN" run tests/valid/unicode.hhy)
case "$unicode_output" in
    "5
HÉLLO
äbc
6") ;;
    *) fail "Unicode length or case conversion is incorrect: $unicode_output" ;;
esac

files_output=$("$HHY_BIN" run tests/valid/files-flow.hhy)
case "$files_output" in
    '["one.log", "root.log"]'|'["root.log", "one.log"]') ;;
    *) fail "directory Flow returned unexpected content: $files_output" ;;
esac

system_metadata_output=$("$HHY_BIN" run tests/valid/system-metadata.hhy)
case "$system_metadata_output" in
    "true
DateTime
DateTime
true
true
Path"|"true
Null
DateTime
true
true
Path") ;;
    *) fail "read_bytes, File metadata or system fields are incorrect: $system_metadata_output" ;;
esac

# The five normative v1.0 acceptance scenarios from docs/HHY_V1.md run with
# deterministic local fixtures rather than external services or host thresholds.
rm -rf tests/output/acceptance
mkdir -p tests/output/acceptance/logs tests/output/acceptance/watch
awk 'BEGIN { for (i = 0; i < 90000; i++) print "INFO regular line"; print "ERROR acceptance" }' \
    >tests/output/acceptance/logs/application.log
"$HHY_BIN" run tests/acceptance/01-file-text-units-parallel.hhy \
    tests/output/acceptance/logs tests/output/acceptance/errors.txt
[ "$(cat tests/output/acceptance/errors.txt)" = "ERROR acceptance" ] ||
    fail "file/text/units/parallel acceptance scenario failed"

if [ "$process_snapshot_available" -eq 1 ]; then
  acceptance_process_output=$("$HHY_BIN" run tests/acceptance/03-process-system.hhy)
  case "$acceptance_process_output" in
    ''|*[!0-9]*) fail "process/system acceptance scenario returned invalid count" ;;
      0) fail "process/system acceptance scenario found no processes" ;;
  esac
else
  printf '%s\n' 'SKIP[HHY_CAP_PROCESS_SNAPSHOT]: host denies /bin/ps; process/system acceptance is covered by CI'
fi

acceptance_language_output=$("$HHY_BIN" run tests/acceptance/05-language-modules.hhy)
[ "$acceptance_language_output" = '{"count": 3, "total": 40}' ] ||
    fail "language/module acceptance scenario failed: $acceptance_language_output"

"$HHY_BIN" run tests/acceptance/04-watch-automation.hhy \
    tests/output/acceptance/watch tests/output/acceptance/watch-ready \
    >tests/output/acceptance/watch.log 2>&1 &
acceptance_watch_pid=$!
attempts=0
while [ ! -s tests/output/acceptance/watch-ready ] && [ "$attempts" -lt 200 ]; do
    sleep 0.05
    attempts=$((attempts + 1))
done
[ -s tests/output/acceptance/watch-ready ] || fail "acceptance watch did not signal readiness"
sleep 0.1
event_attempts=0
while kill -0 "$acceptance_watch_pid" 2>/dev/null && [ "$event_attempts" -lt 50 ]; do
    "$HHY_BIN" run tests/valid/watch-write.hhy tests/output/acceptance/watch/main.c \
        "int main(void) { return $event_attempts; }"
    sleep 0.05
    event_attempts=$((event_attempts + 1))
done
set +e
wait "$acceptance_watch_pid"
acceptance_watch_status=$?
set -e
[ "$acceptance_watch_status" -eq 0 ] || fail "watch/automation acceptance scenario failed"

if [ "$socket_bind_available" -eq 1 ]; then
    port_file=tests/output/http-port
    rm -f "$port_file"
    python3 -u tests/http_server.py "$port_file" >tests/output/http-server.log 2>&1 &
    server_pid=$!
    trap 'kill "$server_pid" 2>/dev/null || true' EXIT INT TERM
    attempts=0
    while [ ! -s "$port_file" ] && kill -0 "$server_pid" 2>/dev/null &&
          [ "$attempts" -lt 200 ]; do
        sleep 0.05
        attempts=$((attempts + 1))
    done
    if [ ! -s "$port_file" ]; then
        kill "$server_pid" 2>/dev/null || true
        set +e
        wait "$server_pid"
        server_status=$?
        set -e
        server_log=$(cat tests/output/http-server.log 2>/dev/null || true)
        python_description=$(python3 --version 2>&1 || true)
        fail "local HTTP test server did not start (status=$server_status, python=$python_description): $server_log"
    fi
    http_port=$(sed -n '1p' "$port_file")
    http_output=$("$HHY_BIN" run tests/valid/http-flow.hhy "http://127.0.0.1:$http_port/users.json")
    case "$http_output" in
        "2") ;;
        *) fail "HTTP JSON Flow returned unexpected content: $http_output" ;;
    esac
    rm -f tests/output/http-stream.json
    http_stream_output=$("$HHY_BIN" run tests/valid/http-stream-to-file.hhy \
        "http://127.0.0.1:$http_port/users.json" tests/output/http-stream.json)
    [ "$http_stream_output" = "2" ] || fail "streaming HTTP output returned unexpected content"
    [ -s tests/output/http-stream.json ] || fail "streaming HTTP output file was not written"
    resource_unwind_output=$("$HHY_BIN" run --limit max_open_files=4 --limit max_processes=1 \
        tests/valid/resource-unwind.hhy "http://127.0.0.1:$http_port")
    [ "$resource_unwind_output" = "true
0
true" ] || fail "file/process/network failure paths leaked resources: $resource_unwind_output"
    "$HHY_BIN" run tests/acceptance/02-http-json-error.hhy \
        "http://127.0.0.1:$http_port/users.json" tests/output/acceptance/active-users.json
    acceptance_http_json=$(cat tests/output/acceptance/active-users.json)
    case "$acceptance_http_json" in
        *'"Ada"'*'"Grace"'*) ;;
        *) fail "HTTP/JSON/error acceptance scenario failed: $acceptance_http_json" ;;
    esac
    case "$acceptance_http_json" in
        *'"Linus"'*) fail "HTTP acceptance failed to filter inactive user" ;;
    esac
    set +e
    http_error_output=$("$HHY_BIN" run tests/valid/http-status-error.hhy "http://127.0.0.1:$http_port/missing" 2>&1)
    http_error_status=$?
    set -e
    if [ "$http_error_status" -ne 4 ]; then
        fail "response_body did not return external I/O status 4"
    fi
    case "$http_error_output" in
        *"status 404"*) ;;
        *) fail "HTTP status error did not include status: $http_error_output" ;;
    esac
    http_options_output=$("$HHY_BIN" run tests/valid/http-options.hhy "http://127.0.0.1:$http_port")
    case "$http_options_output" in
        "HHY flow
true
2
query-header
Pipe Everything.
body-header") ;;
        *) fail "HTTP query/header/body options are incorrect: $http_options_output" ;;
    esac
    rm -f tests/output/http-bytes.bin
    http_bytes_output=$("$HHY_BIN" run tests/valid/http-bytes.hhy \
        "http://127.0.0.1:$http_port" tests/output/http-bytes.bin)
    [ "$http_bytes_output" = "6
EncodingError" ] || fail "HTTP binary response contract is incorrect: $http_bytes_output"
    python3 - <<'PY' || fail "write_bytes did not preserve binary HTTP content"
from pathlib import Path
assert Path("tests/output/http-bytes.bin").read_bytes() == b"\x00HHY\xff\n"
PY
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    trap - EXIT INT TERM
elif ! command -v python3 >/dev/null 2>&1; then
    printf '%s\n' 'SKIP[HHY_CAP_PYTHON]: python3 is unavailable; local HTTP acceptance is covered by CI'
else
    socket_reason=$(python3 tests/capabilities.py socket-bind 2>&1 || true)
    printf '%s\n' "SKIP[HHY_CAP_SOCKET_BIND]: $socket_reason; local HTTP acceptance is covered by CI"
fi

set +e
exit_output=$("$HHY_BIN" run tests/valid/exit-from-function.hhy 2>&1)
exit_status=$?
set -e
if [ "$exit_status" -ne 7 ] || [ -n "$exit_output" ]; then
    fail "exit did not terminate through function boundary"
fi

for source in tests/invalid-runtime/*.hhy; do
    if "$HHY_BIN" run "$source" >/dev/null 2>&1; then
        fail "expected runtime to reject $source"
    fi
done

set +e
printf 'print("\377")\n' > tests/output/invalid-utf8.hhy
"$HHY_BIN" check tests/output/invalid-utf8.hhy >/dev/null 2>&1
utf8_syntax_status=$?
"$HHY_BIN" check tests/invalid/missing-expression.hhy >/dev/null 2>&1
syntax_status=$?
"$HHY_BIN" run tests/invalid-runtime/immutable-assignment.hhy >/dev/null 2>&1
runtime_status=$?
"$HHY_BIN" run tests/does-not-exist.hhy >/dev/null 2>&1
io_status=$?
"$HHY_BIN" unknown-command >/dev/null 2>&1
usage_status=$?
set -e
rm -f tests/output/invalid-utf8.hhy
if [ "$utf8_syntax_status" -ne 2 ] || [ "$syntax_status" -ne 2 ] || [ "$runtime_status" -ne 1 ] ||
   [ "$io_status" -ne 4 ] || [ "$usage_status" -ne 3 ]; then
    fail "CLI exit code contract is incorrect"
fi

echo "all lexer, parser and runtime tests passed"
