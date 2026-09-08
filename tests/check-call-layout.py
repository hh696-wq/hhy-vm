#!/usr/bin/env python3
"""AST / legacy Bytecode / verified call-layout conformance, including unwind."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

# Bounded completion oracle; the million-item fixture remains in the timeout-cancellation suite.
binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else 'build/hhy').resolve())
CASES = [
    ('tests/valid/call-layout.hhy', []),
    ('tests/valid/call-unwind.hhy', []),
    ('tests/valid/frame-slots-escape.hhy', []),
    ('tests/valid/stack-trace.hhy', []),
    ('tests/valid/bytecode-specialization-fallback.hhy', []),
    ('tests/valid/bytecode-specialization-distinct.hhy', []),
    ('tests/valid/exit-from-function.hhy', []),
    ('tests/invalid-runtime/recursion-limit.hhy', ['--limit', 'max_recursion=8']),
    ('tests/invalid-runtime/bytecode-specialization-overflow.hhy', []),
    ('tests/invalid-runtime/bytecode-specialization-div-zero.hhy', []),
]
with tempfile.TemporaryDirectory(prefix='hhy-call-layout-') as temporary:
    report = Path(temporary) / 'profile.json'
    for source, options in CASES:
        oracle = subprocess.run([binary, 'run', '--engine', 'ast', *options, source], capture_output=True, timeout=60)
        expected_status = 7 if source.endswith('exit-from-function.hhy') else (1 if '/invalid-runtime/' in source else 0)
        assert oracle.returncode == expected_status, (source, oracle.stderr)
        if source.endswith('/call-layout.hhy'):
            assert oracle.stdout == b'12\n12\n10\n32\n6\n7\n15\nTypeError\n10\n'
        if source.endswith('/call-unwind.hhy'):
            assert oracle.stdout.splitlines()[-4:] == [b'42', b'null', b'null', b'9']
        for gate in ('0', '1'):
            env = {**os.environ, 'HHY_BYTECODE_CALL_PLANS': gate}
            for mode in ('run', 'profile'):
                command = [binary, mode, '--engine', 'bytecode', *options]
                if mode == 'profile':
                    command += ['--cpu', '--heap', '--format', 'json', '--output', str(report)]
                report.unlink(missing_ok=True)
                run = subprocess.run([*command, source], env=env, capture_output=True, timeout=60)
                assert (run.returncode, run.stdout, run.stderr) == (oracle.returncode, oracle.stdout, oracle.stderr), (source, gate, mode, run.stderr)
                if mode == 'profile':
                    assert report.exists(), (source, gate, run.stderr)
                    layout = json.loads(report.read_text())['call_layout']
                    assert layout['schema_version'] == layout['plan_version'] == 1
                    if gate == '0':
                        assert layout['selected_calls'] == 0, (source, gate, layout)
                    elif source == 'tests/valid/call-layout.hhy':
                        assert layout['selected_calls'] > 40 and layout['generic_calls'] == 0, layout
    tiny = Path(temporary) / 'closure.hhy'
    tiny.write_text('fn identity(ignored, callback) { return callback }\nlet closure = null |> identity { value -> value + 1 }\nprint(closure(2))\n')
    subprocess.run([binary, 'profile', '--engine', 'bytecode', '--format', 'json', '--output', str(report), str(tiny)],
                   env={**os.environ, 'HHY_BYTECODE_CALL_PLANS': '1'}, check=True, capture_output=True, timeout=60)
    layout = json.loads(report.read_text())['call_layout']
    assert layout['selected_calls'] == 2 and layout['generic_calls'] == 0, layout
    for gate in ('0', '1'):
        fault = subprocess.run([binary, 'run', '--engine', 'bytecode', str(tiny)], capture_output=True,
            env={**os.environ, 'HHY_BYTECODE_CALL_PLANS': gate, 'HHY_TEST_BYTECODE_FAULT': 'invalid-call-plan'})
        assert fault.returncode == 2 and b'call plan' in fault.stderr and not fault.stdout, fault.stderr
    # Release-only forced GC is run separately, avoiding ASan fake-stack conflicts.
    dump = subprocess.check_output([binary, 'bytecode', 'tests/valid/call-layout.hhy'], text=True)
    assert 'call_plans ' in dump and 'call_plan source=' in dump
    metrics = json.loads(subprocess.check_output([binary, 'bytecode', '--metrics', 'tests/valid/call-layout.hhy']))
    assert metrics['call_plan_version'] == 1
    assert metrics['call_plans'] == sum(line.startswith('call_plan source=') for line in dump.splitlines())
    assert metrics['call_plan_bytes'] > 0
print('call layout: AST/legacy/plan, profiler, captures, recursion and errors passed')
