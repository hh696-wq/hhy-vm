#!/usr/bin/env python3
"""Run host-fatal regressions in isolated processes; success requires recovery."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--binary', default='build/hhy-embed-unwind-test')
parser.add_argument('--output', default='build/embed-unwind.json')
args = parser.parse_args()
results = []
with tempfile.TemporaryDirectory(prefix='hhy-embed-unwind-') as temp:
    for mode in ('arguments', 'files', 'atomic', 'errors', 'time', 'init'):
        for engine in ('ast', 'bytecode'):
            for gate in ('0', '1') if engine == 'bytecode' else ('0',):
                directory = Path(temp) / f'{mode}-{engine}-{gate}'
                directory.mkdir()
                run = subprocess.run([args.binary, mode, engine, str(directory)],
                    env={**os.environ, 'HHY_BYTECODE_CALL_PLANS': gate},
                    text=True, capture_output=True, timeout=60)
                results.append({'mode': mode, 'engine': engine, 'call_plans': gate,
                                'exit_code': run.returncode, 'stdout': run.stdout, 'stderr': run.stderr})
output = Path(args.output)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps({'schema_version': 1, 'cases': results,
                             'passed': all(r['exit_code'] == 0 for r in results)}, indent=2) + '\n')
failed = [r for r in results if r['exit_code'] != 0]
if failed:
    raise SystemExit(json.dumps(failed, indent=2))
print(f'Embedding unwind: {len(results)} isolated AST/Bytecode/plan recovery cases passed')
