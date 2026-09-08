#!/usr/bin/env python3
"""Whole-program AST/direct Bytecode/IR-off/IR-on and independent-pass matrix."""
import itertools
import json
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else 'build/hhy').resolve())
passes = ['fold', 'constant-propagation', 'copy-propagation', 'peephole', 'unreachable', 'dce']
env = {k: v for k, v in os.environ.items() if not k.startswith('HHY_COMPILER')}
rng = random.Random(1703)
count = 0

def run(source, compiler='', disabled='', engine='bytecode', options=()):
    global count
    count += 1
    r = subprocess.run([binary, 'run', '--engine', engine, *options, str(source)],
                       env={**env, 'HHY_COMPILER': compiler or 'bc', 'HHY_COMPILER_DISABLE': disabled},
                       capture_output=True, timeout=30)
    return r.returncode, r.stdout, r.stderr

def expression(depth):
    if not depth or rng.random() < .3:
        return str(rng.randrange(101))
    return '(' + expression(depth-1) + rng.choice([' + ', ' - ', ' * ', ' % ']) + expression(depth-1) + ')'

with tempfile.TemporaryDirectory(prefix='hhy-compiler-') as temp:
    temp = Path(temp)
    source = temp/'program.hhy'
    expressions = [expression(4) for _ in range(120)] + [
        '9223372036854775807 + 1', '-(0 - 9223372036854775807 - 1)',
        '(0 - 9223372036854775807 - 1) % -1', '1 % 0',
        '(9223372036854775807 + 1) + (1 % 0)', '(1 % 0) + (9223372036854775807 + 1)',
        '9007199254740992 == 9007199254740993', '9007199254740992 != 9007199254740993',
        '9223372036854775807 > 9223372036854775806', '0x7fffffffffffffff - 1',
        '0b1010 + 1_000', '1 / 2', '1.0 + 2.0', '"a" + "b"', 'false and (1 % 0)',
        'true or (1 % 0)', 'true and 1', '9223372036854775808', '0' * 130 + '1',
        '(0 - 9223372036854775807) - 1', '-9223372036854775807', '+(4 + 2)']
    for expr in expressions:
        source.write_text('print(' + expr + ')\n')
        oracle = run(source, engine='ast')
        for compiler, disabled in [('', ''), ('ir', 'all'), ('ir', '')]:
            observed = run(source, compiler, disabled)
            assert observed == oracle, (expr, compiler, disabled, oracle, observed)

    programs = [
        'let a = 10\nlet b = a\nprint(b + (2 * 3))\n',
        'fn f() {\n 1 + 2\n return 4 + 0\n print("dead")\n}\nprint(f())\n',
        'fn f(x) { if x { return 1 } else { return 2 }\n throw("dead") }\nprint(f(false))\n',
        'let mut a = 1\nlet b = a\na = 2\nprint(b)\nprint(a)\n',
        'let a = 1\nif true { let a = 2\nprint(a) }\nprint(a)\n',
        'try { print(1 % 0) } catch e { print(e) }\nprint(4 + 5)\n',
        'let mut i = 0\nwhile i < 3 { i = i + 1\n if i == 2 { continue }\n print(i) }\n',
        'for x in [1, 2, 3] { if x == 2 { break }\n print(x) }\n',
        'print([1, 2] |> stream |> map { x -> 4 + 2 } |> collect)\n',
        'print([1, 2] |> stream |> map { x -> } |> collect)\n',
        'print([1, 2] |> stream |> map {} |> collect)\n',
        'fn f() { 42 }\nprint(f())\n',
        'if false { throw(1) } else { print(2 + 3) }\nwhile false { throw(2) }\n',
        'if true { print(4) } else if true { throw(3) }\n',
        'fn f() { 1\n 42 }\nprint(f())\n',
        'fn identity(x, f) { return f }\nfn f() { let a = 10\n return null |> identity { x -> a + x } }\nprint(f()(2))\n',
    ]
    for text in programs:
        source.write_text(text)
        oracle = run(source, engine='ast')
        for bits in itertools.product((False, True), repeat=len(passes)):
            disabled = ','.join(p for p, active in zip(passes, bits) if not active)
            observed = run(source, 'ir', disabled)
            assert observed == oracle, (text, disabled, oracle, observed)

    fixtures = [
        'tests/valid/basic.hhy', 'tests/valid/frame-slots-escape.hhy',
        'tests/valid/closure-unwind-matrix.hhy', 'tests/valid/exception-regions.hhy',
        'tests/valid/structured-error.hhy', 'tests/valid/stack-trace.hhy',
        'tests/valid/advanced-flow.hhy', 'tests/valid/json-flow.hhy', 'tests/valid/csv-flow.hhy',
        'tests/valid/int64-exact.hhy', 'tests/invalid-runtime/recursion-limit.hhy',
        'tests/invalid-runtime/int-overflow-add.hhy',
    ]
    for fixture in fixtures:
        oracle = run(fixture, engine='ast')
        for compiler, disabled in [('', ''), ('ir', 'all'), ('ir', '')]:
            assert run(fixture, compiler, disabled) == oracle, fixture

    # Quota outcome, including full diagnostic/source text, must agree with the
    # direct Bytecode oracle at boundary values; optimization never removes allocs.
    source.write_text('fn f(x) { let a = 100\nlet b = a\n1 + 2\nreturn [b + 3, x] }\nprint(f(4))\n')
    for limit in ['max_memory=1000', 'max_memory=10000', 'max_memory=100000',
                  'max_recursion=1', 'max_recursion=2', 'max_open_files=1']:
        oracle = run(source, options=('--limit', limit))
        assert run(source, 'ir', options=('--limit', limit)) == oracle, (limit, oracle)

    # Whole-program no-pass lowering must emit byte-for-byte identical disassembly,
    # including callable layouts, exception tables, kernels and resolved slots.
    compiled = 0
    for pattern in ('examples/*.hhy', 'tests/valid/*.hhy', 'tests/acceptance/*.hhy', 'tests/acceptance/lib/*.hhy'):
        for fixture in sorted(Path('.').glob(pattern)):
            def disasm(mode, disabled):
                return subprocess.run([binary, 'bytecode', str(fixture)], capture_output=True, timeout=30,
                    env={**env, 'HHY_COMPILER': mode, 'HHY_COMPILER_DISABLE': disabled})
            baseline, lowered = disasm('', ''), disasm('ir', 'all')
            assert (baseline.returncode, baseline.stdout, baseline.stderr) == (lowered.returncode, lowered.stdout, lowered.stderr), fixture
            optimized = disasm('ir', '')
            assert optimized.returncode == baseline.returncode, (fixture, optimized.stderr)
            compiled += 1
print(json.dumps({'status': 'passed', 'seed': 1703, 'execution_paths': count,
                  'expressions': len(expressions), 'all_64_pass_combinations_programs': len(programs),
                  'whole_program_compile_fixtures': compiled}))
