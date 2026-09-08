#!/usr/bin/env python3
"""Exact switch-entry accounting and opt-in semantic differential checks."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

# Bounded completion oracle; the million-item fixture remains in the timeout-cancellation suite.
binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else 'build/hhy').resolve())
with tempfile.TemporaryDirectory(prefix='hhy-dispatch-') as directory:
    report = Path(directory) / 'profile.json'
    tiny = Path(directory) / 'tiny.hhy'
    tiny.write_text('print(1)\n')
    sources = [tiny, Path('benchmarks/opcode-loop.hhy'),
               Path('tests/valid/bytecode-specialization-fallback.hhy'),
               Path('tests/valid/bytecode-specialization-distinct.hhy'),
               Path('tests/invalid-runtime/bytecode-specialization-div-zero.hhy'),
               Path('tests/invalid-runtime/bytecode-specialization-overflow.hhy')]
    for source in sources:
        oracle = subprocess.run([binary, 'run', '--engine', 'ast', str(source)], capture_output=True, timeout=60)
        for engine, gate in [('ast', '1'), ('bytecode', '0'), ('bytecode', '1')]:
            env = {**os.environ, 'HHY_PROFILE_DISPATCH': gate}
            run = subprocess.run([binary, 'profile', '--engine', engine, '--cpu', '--heap',
                                  '--format', 'json', '--output', str(report), str(source)],
                                 env=env, capture_output=True, timeout=60)
            assert (run.returncode, run.stdout, run.stderr) == (oracle.returncode, oracle.stdout, oracle.stderr), (source, engine, gate, run.stderr)
            data = json.loads(report.read_text())['dispatch_profile']
            enabled = engine == 'bytecode' and gate == '1'
            assert data['status'] == ('enabled' if enabled else 'disabled')
            counts = {tuple(x['opcodes']): x['count'] for x in data['sequences']}
            assert sum(v for k, v in counts.items() if len(k) == 1) == data['total']
            assert data['transitions_are_fusion_candidates'] is False
            if not enabled:
                assert data['total'] == data['storage_bytes'] == 0 and not counts
            if source == tiny and enabled:
                names = ['PROGRAM', 'EXPR_STMT', 'CALL', 'IDENTIFIER', 'LITERAL']
                # Use the disassembler's opcode spelling as the schema contract.
                assert data['total'] == 5, data
                for length in (1, 2, 3):
                    assert sum(v for k, v in counts.items() if len(k) == length) == 6-length
                expected = {tuple(names[i:i+length]): 1 for length in (1, 2, 3) for i in range(len(names)-length+1)}
                assert counts == expected, counts
print('dispatch profile: exact counts, opt-out, AST, fallback and errors passed')
