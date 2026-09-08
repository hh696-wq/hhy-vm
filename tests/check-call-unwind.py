#!/usr/bin/env python3
"""Stage 1.6.1 semantic matrix, profiler balance and real cancellation."""
import itertools
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else 'build/hhy').resolve())
flags = ('HHY_BYTECODE_CALL_PLANS', 'HHY_BYTECODE_EXCEPTION_TABLES', 'HHY_CALL_FRAME_UNWIND')
base = {**os.environ, **{key: '0' for key in flags}, 'HHY_CALL_FRAME_POOL': 'legacy'}
count = 0
with tempfile.TemporaryDirectory(prefix='hhy-call-unwind-') as temp:
    temp = Path(temp); report = temp/'profile.json'
    memory = temp/'memory.hhy'
    memory.write_text('fn blow(n) {\nif n == 0 { return range(0, 1000000) |> stream |> collect }\nreturn blow(n - 1)\n}\nblow(12)\n')
    timeout = temp/'timeout.hhy'
    timeout.write_text('fn spin(n) {\nif n == 0 { while true {} }\nreturn spin(n - 1)\n}\nspin(8)\n')
    cases = [('tests/valid/closure-unwind-matrix.hhy', [], 0),
             ('tests/valid/call-unwind.hhy', [], 0),
             ('tests/valid/exception-regions.hhy', [], 0),
             ('tests/valid/frame-pool.hhy', [], 0),
             ('tests/invalid-runtime/recursion-limit.hhy', ['--limit', 'max_recursion=8'], 1),
             (str(memory), ['--limit', 'max_memory=256kib'], 1),
             (str(timeout), ['--limit', 'max_runtime=10ms'], 5)]
    for source, options, expected_status in cases:
        oracle = subprocess.run([binary, 'run', '--engine', 'ast', *options, source], env=base,
                                capture_output=True, timeout=60)
        assert oracle.returncode == expected_status, (source, oracle.stderr)
        if source.endswith('closure-unwind-matrix.hhy'):
            assert oracle.stdout == b'12\n12\n103\n12\ntrue\ntrue\n42\n[8, 9, 10]\n'
        for engine in ('ast', 'bytecode'):
            for calls, tables, unwind, pool in itertools.product(('0', '1'), repeat=4):
                env = {**base, flags[0]: calls, flags[1]: tables, flags[2]: unwind,
                       'HHY_CALL_FRAME_POOL': 'bounded' if pool == '1' else 'legacy'}
                for mode in ('run', 'profile'):
                    command = [binary, mode, '--engine', engine, *options]
                    if mode == 'profile': command += ['--heap', '--format', 'json', '--output', str(report)]
                    report.unlink(missing_ok=True)
                    run = subprocess.run([*command, source], env=env, capture_output=True, timeout=60)
                    assert (run.returncode, run.stdout, run.stderr) == (oracle.returncode, oracle.stdout, oracle.stderr), (
                        source, engine, calls, tables, unwind, pool, mode, run.stderr, oracle.stderr)
                    count += 1
                    if mode == 'profile':
                        stats = json.loads(report.read_text())['call_unwind']
                        assert stats['schema_version'] == stats['table_version'] == 1
                        assert stats['enabled'] == (unwind == '1') and stats['active'] == 0, stats
                        assert stats['pushed'] == sum(stats[k] for k in ('returned', 'errors', 'cancelled', 'resources')), stats
                        if unwind == '1':
                            assert stats['pushed'] > 0 and stats['peak_active'] > 0, stats
                            if source == str(memory): assert stats['resources'] > 0, stats
                            if source == str(timeout): assert stats['cancelled'] > 0, stats
                        else: assert stats['pushed'] == 0 and stats['reserved_bytes'] == 0, stats
    # SIGINT after initialization and nested calls, not a startup race.
    ready = temp/'ready'; cancellation = temp/'signal.hhy'
    cancellation.write_text('fn spin(n) {\nif n == 0 { write_text(path(args[0]), "ready"); while true {} }\nreturn spin(n - 1)\n}\nspin(8)\n')
    for engine in ('ast', 'bytecode'):
        ready.unlink(missing_ok=True); report.unlink(missing_ok=True)
        process = subprocess.Popen([binary, 'profile', '--engine', engine, '--format', 'json',
            '--output', str(report), str(cancellation), str(ready)], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env={**base, **{k: '1' for k in flags}, 'HHY_CALL_FRAME_POOL': 'bounded'}, start_new_session=True)
        try:
            deadline = time.monotonic() + 10
            while not ready.exists():
                assert process.poll() is None, process.communicate()
                assert time.monotonic() < deadline, 'initialization timeout'
                time.sleep(.01)
            time.sleep(.05); process.send_signal(signal.SIGINT)
            stdout, stderr = process.communicate(timeout=10)
            assert process.returncode == 5 and b'CancelledError: execution cancelled' in stderr and not stdout, (stdout, stderr)
            stats = json.loads(report.read_text())['call_unwind']
            assert stats['active'] == 0 and stats['cancelled'] >= 9, stats
        finally:
            if process.poll() is None: process.kill()
            process.communicate(timeout=5)
print(f'v1.6.1: {count} differential runs across 32 engine/switch combinations and nested SIGINT passed')
