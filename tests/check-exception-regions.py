#!/usr/bin/env python3
"""Differential checks for verified exception layouts and independent switches."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else 'build/hhy').resolve())
fixture = 'tests/valid/exception-regions.hhy'
cases = [
    (fixture, [], 0),
    ('tests/valid/call-unwind.hhy', [], 0),
    ('tests/valid/stack-trace.hhy', [], 0),
    ('tests/valid/bytecode-specialization-cancel.hhy', [], 0),
    ('tests/invalid-runtime/recursion-limit.hhy', ['--limit', 'max_recursion=8'], 1),
    ('tests/acceptance/embed-init-memory.hhy', ['--limit', 'max_memory=256kib'], 1),
]
with tempfile.TemporaryDirectory(prefix='hhy-exception-regions-') as temporary:
    report = Path(temporary) / 'profile.json'
    for source, options, status in cases:
        oracle = subprocess.run([binary, 'run', '--engine', 'ast', *options, source], capture_output=True, timeout=60)
        assert oracle.returncode == status, (source, oracle.stderr)
        if source == fixture:
            assert oracle.stdout == b'deep\nrethrown\n42\n17\nfalse\nattempted\n2\nroot-survives\n'
        for tables in ('0', '1'):
            for calls in ('0', '1'):
                env = {**os.environ, 'HHY_BYTECODE_EXCEPTION_TABLES': tables, 'HHY_BYTECODE_CALL_PLANS': calls}
                for mode in ('run', 'profile'):
                    command = [binary, mode, '--engine', 'bytecode', *options]
                    if mode == 'profile': command += ['--heap', '--format', 'json', '--output', str(report)]
                    report.unlink(missing_ok=True)
                    run = subprocess.run([*command, source], env=env, capture_output=True, timeout=60)
                    assert (run.returncode, run.stdout, run.stderr) == (oracle.returncode, oracle.stdout, oracle.stderr), (source, tables, calls, mode, run.stderr)
                    if mode == 'profile' and source == fixture:
                        layout = json.loads(report.read_text())['exception_layout']
                        assert layout['schema_version'] == layout['table_version'] == 1
                        assert layout['selected_regions' if tables == '1' else 'generic_regions'] > 8, layout
                        assert layout['generic_regions' if tables == '1' else 'selected_regions'] == 0, layout
    for tables in ('0', '1'):
        run = subprocess.run([binary, 'run', fixture], capture_output=True,
            env={**os.environ, 'HHY_BYTECODE_EXCEPTION_TABLES': tables,
                 'HHY_TEST_BYTECODE_FAULT': 'invalid-exception-region'}, timeout=60)
        assert run.returncode == 2 and b'exception region' in run.stderr and not run.stdout, run.stderr
    dump = subprocess.check_output([binary, 'bytecode', fixture], text=True)
    metrics = json.loads(subprocess.check_output([binary, 'bytecode', '--metrics', fixture]))
    assert metrics['exception_table_version'] == 1
    assert metrics['exception_regions'] == sum(x.startswith('exception_region source=') for x in dump.splitlines())
    assert metrics['exception_regions'] >= 8 and metrics['exception_table_bytes'] > 0
print('exception regions: 48 differential runs, profiler, scopes, limits and fault rejection passed')
