#!/usr/bin/env python3
"""Release-only forced-GC roots under every registered-unwind companion switch."""
import itertools
import os
from pathlib import Path
import subprocess
import sys

binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else 'build/hhy').resolve())
count = 0
for source in ('tests/valid/closure-unwind-matrix.hhy', 'tests/valid/call-unwind.hhy', 'tests/valid/frame-pool.hhy'):
    oracle = subprocess.run([binary, 'run', '--engine', 'ast', source], capture_output=True, check=True,
        env={**os.environ, 'HHY_GC_STRESS': '0', 'HHY_CALL_FRAME_UNWIND': '0', 'HHY_CALL_FRAME_POOL': 'legacy'}, timeout=60)
    for engine in ('ast', 'bytecode'):
        for calls, tables, pool in itertools.product(('0', '1'), repeat=3):
            run = subprocess.run([binary, 'run', '--engine', engine, source], capture_output=True,
                env={**os.environ, 'HHY_GC_STRESS': '1', 'HHY_CALL_FRAME_UNWIND': '1',
                     'HHY_BYTECODE_CALL_PLANS': calls, 'HHY_BYTECODE_EXCEPTION_TABLES': tables,
                     'HHY_CALL_FRAME_POOL': 'bounded' if pool == '1' else 'legacy'}, timeout=60)
            assert (run.returncode,run.stdout,run.stderr) == (oracle.returncode,oracle.stdout,oracle.stderr), (source,engine,calls,tables,pool,run.stderr)
            count += 1
print(f'registered call roots: {count} forced-GC cases passed (Release only)')
