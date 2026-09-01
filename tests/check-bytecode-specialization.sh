#!/bin/sh
set -eu

HHY_BIN=${1:-build/hhy}
source_file=tests/valid/bytecode-specialization.hhy
specialization_tmp=$(mktemp -d)
trap 'rm -rf "$specialization_tmp"' EXIT INT TERM

fail() {
    echo "bytecode specialization test failure: $1" >&2
    exit 1
}

"$HHY_BIN" run --engine ast "$source_file" >"$specialization_tmp/ast.out"
HHY_BYTECODE_SPECIALIZATION=off \
    "$HHY_BIN" run --engine bytecode "$source_file" >"$specialization_tmp/general.out"
HHY_BYTECODE_SPECIALIZATION_REPORT=json \
    "$HHY_BIN" run --engine bytecode "$source_file" \
    >"$specialization_tmp/specialized.out" 2>"$specialization_tmp/specialized.jsonl"

cmp -s "$specialization_tmp/ast.out" "$specialization_tmp/general.out" ||
    fail "AST and general Bytecode output differ"
cmp -s "$specialization_tmp/ast.out" "$specialization_tmp/specialized.out" ||
    fail "AST and specialized Bytecode output differ"

selected_count=$(grep -c '"selected":true,"reason":"selected"' \
    "$specialization_tmp/specialized.jsonl" || true)
[ "$selected_count" -eq 2 ] ||
    fail "expected two selected specialization reports, got $selected_count"
grep -q '"operations":\["multiply_int_checked","modulo_equals_int_checked","distinct_stable"\]' \
    "$specialization_tmp/specialized.jsonl" ||
    fail "selected report does not expose verified operation metadata"

HHY_BYTECODE_SPECIALIZATION=off HHY_BYTECODE_SPECIALIZATION_REPORT=json \
    "$HHY_BIN" run --engine bytecode "$source_file" \
    >/dev/null 2>"$specialization_tmp/disabled.jsonl"
disabled_count=$(grep -c '"selected":false,"reason":"disabled"' \
    "$specialization_tmp/disabled.jsonl" || true)
[ "$disabled_count" -eq 2 ] ||
    fail "expected two disabled fallback reports, got $disabled_count"

for error_source in \
    tests/invalid-runtime/bytecode-specialization-div-zero.hhy \
    tests/invalid-runtime/bytecode-specialization-overflow.hhy
do
    set +e
    "$HHY_BIN" run --engine ast "$error_source" \
        >"$specialization_tmp/error-ast.out" 2>"$specialization_tmp/error-ast.err"
    ast_status=$?
    HHY_BYTECODE_SPECIALIZATION=off "$HHY_BIN" run --engine bytecode "$error_source" \
        >"$specialization_tmp/error-general.out" 2>"$specialization_tmp/error-general.err"
    general_status=$?
    "$HHY_BIN" run --engine bytecode "$error_source" \
        >"$specialization_tmp/error-specialized.out" 2>"$specialization_tmp/error-specialized.err"
    specialized_status=$?
    set -e
    [ "$ast_status" -eq "$general_status" ] &&
        [ "$ast_status" -eq "$specialized_status" ] && [ "$ast_status" -ne 0 ] ||
        fail "error status differs across execution paths for $error_source"
    cmp -s "$specialization_tmp/error-ast.out" "$specialization_tmp/error-general.out" &&
        cmp -s "$specialization_tmp/error-ast.out" "$specialization_tmp/error-specialized.out" ||
        fail "error stdout differs across execution paths for $error_source"
    cmp -s "$specialization_tmp/error-ast.err" "$specialization_tmp/error-general.err" &&
        cmp -s "$specialization_tmp/error-ast.err" "$specialization_tmp/error-specialized.err" ||
        fail "error diagnostics differ across execution paths for $error_source"
done

echo "bytecode specialization three-path and metamorphic tests passed"
