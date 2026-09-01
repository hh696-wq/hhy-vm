#!/bin/sh
set -eu

HHY_BIN=${1:-build/hhy}
conformance_tmp=$(mktemp -d)
trap 'rm -rf "$conformance_tmp"' EXIT INT TERM

compare_case() {
    source=$1
    shift
    set +e
    "$HHY_BIN" run --engine ast "$@" "$source" >"$conformance_tmp/ast.out" 2>"$conformance_tmp/ast.err"
    ast_status=$?
    "$HHY_BIN" run --engine bytecode "$@" "$source" >"$conformance_tmp/bytecode.out" 2>"$conformance_tmp/bytecode.err"
    bytecode_status=$?
    set -e
    [ "$ast_status" -eq "$bytecode_status" ] || {
        echo "engine conformance status mismatch: $source" >&2
        exit 1
    }
    cmp -s "$conformance_tmp/ast.out" "$conformance_tmp/bytecode.out" || {
        echo "engine conformance stdout mismatch: $source" >&2
        exit 1
    }
    cmp -s "$conformance_tmp/ast.err" "$conformance_tmp/bytecode.err" || {
        echo "engine conformance stderr mismatch: $source" >&2
        exit 1
    }
}

for source in \
    tests/valid/basic.hhy \
    tests/valid/frame-slots-escape.hhy \
    tests/valid/caught-error.hhy \
    tests/valid/structured-error.hhy \
    tests/valid/stack-trace.hhy \
    tests/valid/unicode.hhy \
    tests/valid/int64-exact.hhy \
    tests/valid/group-hash-semantics.hhy \
    tests/valid/advanced-flow.hhy \
    tests/invalid-runtime/int-overflow-add.hhy \
    tests/invalid-runtime/recursion-limit.hhy
do
    compare_case "$source"
done

compare_case tests/invalid-runtime/recursion-limit.hhy --limit recursion=8

for fault in invalid-opcode missing-halt
do
    set +e
    HHY_TEST_BYTECODE_FAULT=$fault "$HHY_BIN" run --engine bytecode tests/valid/basic.hhy \
        >"$conformance_tmp/fault.out" 2>"$conformance_tmp/fault.err"
    fault_status=$?
    set -e
    [ "$fault_status" -eq 2 ] || {
        echo "Bytecode fault injection did not fail closed: $fault" >&2
        exit 1
    }
    grep -q 'bytecode runtime error' "$conformance_tmp/fault.err" || {
        echo "Bytecode fault injection lacked a stable diagnostic: $fault" >&2
        exit 1
    }
done
printf '%s\n' 'AST/Bytecode engine conformance tests passed'

"$HHY_BIN" run tests/valid/basic.hhy >"$conformance_tmp/default.out" 2>"$conformance_tmp/default.err"
"$HHY_BIN" run --engine bytecode tests/valid/basic.hhy >"$conformance_tmp/explicit-bytecode.out" 2>"$conformance_tmp/explicit-bytecode.err"
cmp -s "$conformance_tmp/default.out" "$conformance_tmp/explicit-bytecode.out" || {
    echo "default engine output does not match explicit Bytecode" >&2
    exit 1
}
cmp -s "$conformance_tmp/default.err" "$conformance_tmp/explicit-bytecode.err" || {
    echo "default engine diagnostics do not match explicit Bytecode" >&2
    exit 1
}

HHY_ENGINE=ast "$HHY_BIN" run tests/valid/basic.hhy >"$conformance_tmp/fallback.out" 2>"$conformance_tmp/fallback.err"
cmp -s "$conformance_tmp/fallback.out" "$conformance_tmp/explicit-bytecode.out" || {
    echo "AST fallback output does not match Bytecode default" >&2
    exit 1
}
printf '%s\n' 'Bytecode default and AST fallback tests passed'
