#!/usr/bin/env python3
"""Validate bounded reuse, independent VM switches, escape safety and GC accounting."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else 'build/hhy').resolve())
with tempfile.TemporaryDirectory(prefix='hhy-frame-pool-') as temp:
    temp = Path(temp)
    wide = temp / 'wide.hhy'
    wide.write_text('fn wide(n) {\n' + '\n'.join(f'let v{i} = n + {i}' for i in range(100)) +
                    '\nif n == 0 { return v99 }\nreturn wide(n - 1)\n}\nprint(wide(32))\n')
    oversized = temp / 'oversized.hhy'
    oversized.write_text('fn large() {\n' + '\n'.join(f'let v{i} = {i}' for i in range(1500)) +
                        '\nreturn v1499\n}\nprint(large())\n')
    empty = temp / 'empty.hhy'; empty.write_text('print(42)\n')
    cases = [
        ('tests/valid/frame-pool.hhy', b'128\n128\ndeep\n2\n11\n102\n12\n'),
        ('tests/valid/frame-slots-escape.hhy', None),
        ('tests/valid/call-unwind.hhy', None),
        ('tests/valid/exception-regions.hhy', None),
        (str(wide), b'99\n'), (str(oversized), b'1499\n'), (str(empty), b'42\n')]
    for source, expected in cases:
        oracle = subprocess.run([binary, 'run', '--engine', 'ast', source], capture_output=True, timeout=60,
                                env={**os.environ, 'HHY_CALL_FRAME_POOL': 'legacy'})
        assert oracle.returncode == 0, (source, oracle.stderr)
        if expected is not None: assert oracle.stdout == expected, (source, oracle.stdout)
        for engine, calls, tables in (('ast', '0', '0'), ('bytecode', '0', '0'), ('bytecode', '0', '1'),
                                     ('bytecode', '1', '0'), ('bytecode', '1', '1')):
            for pool in ('legacy', 'bounded'):
                report = temp / 'profile.json'; report.unlink(missing_ok=True)
                run = subprocess.run([binary, 'profile', '--engine', engine, '--heap', '--format', 'json',
                    '--output', str(report), source], capture_output=True, timeout=60,
                    env={**os.environ, 'HHY_CALL_FRAME_POOL': pool, 'HHY_BYTECODE_CALL_PLANS': calls,
                         'HHY_BYTECODE_EXCEPTION_TABLES': tables})
                assert (run.returncode, run.stdout, run.stderr) == (oracle.returncode, oracle.stdout, oracle.stderr), (source, engine, pool, run.stderr)
                stats = json.loads(report.read_text())['frame_pool']
                assert stats['schema_version'] == 1 and stats['bounded'] == (pool == 'bounded'), stats
                assert stats['cached'] - stats['reused'] == stats['retained'], stats
                if pool == 'bounded':
                    assert stats['peak_retained'] <= 64 and stats['peak_retained_gc_bytes'] <= 65536, stats
                if source.endswith('/frame-pool.hhy'):
                    assert stats['reused'] > 0 and stats['escaped'] >= 2, stats
                    assert (stats['discarded'] > 0) == (pool == 'bounded'), stats
                    if pool == 'legacy': assert stats['peak_retained'] > 64, stats
                if source == str(wide):
                    assert (stats['discarded'] > 0) == (pool == 'bounded'), stats
                    if pool == 'legacy': assert stats['peak_retained_gc_bytes'] > 65536, stats
                if source == str(oversized) and pool == 'bounded':
                    assert stats['discarded'] == 1 and stats['retained'] == 0, stats
print('frame pool: 70 profiled differential runs, count/byte bounds, oversized frames and escapes passed')
