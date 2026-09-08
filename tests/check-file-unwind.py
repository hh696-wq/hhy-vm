#!/usr/bin/env python3
"""Isolated OS write failure and quota-longjmp resource ownership checks."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--binary', default='build/hhy-file-unwind-test')
parser.add_argument('--output', default='build/file-unwind.json')
args = parser.parse_args()
results = []
with tempfile.TemporaryDirectory(prefix='hhy-file-unwind-') as temp:
    for mode in ('write-failure', 'write-quota', 'copy-quota'):
        scratch = Path(temp) / mode; scratch.mkdir()
        run = subprocess.run([args.binary, mode, str(scratch)], capture_output=True, text=True, timeout=60)
        results.append({'mode': mode, 'exit_code': run.returncode, 'stdout': run.stdout, 'stderr': run.stderr})
output = Path(args.output); output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps({'schema_version': 1, 'cases': results, 'passed': all(r['exit_code'] == 0 for r in results)}, indent=2) + '\n')
failures = [r for r in results if r['exit_code'] != 0]
if failures: raise SystemExit(json.dumps(failures, indent=2))
print('file unwind: OS write failure, raw fd quota unwind and copy ownership passed')
