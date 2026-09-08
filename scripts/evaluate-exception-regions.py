#!/usr/bin/env python3
"""Local paired runtime/compile evidence; does not grant default enablement."""
import argparse
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument('--binary', default='build/hhy')
parser.add_argument('--baseline', required=True)
parser.add_argument('--iterations', type=int, default=15)
parser.add_argument('--output', default='build/exception-region-evaluation.json')
args = parser.parse_args()
if args.iterations < 3:
    parser.error('at least three paired iterations are required')
binary, baseline = str(Path(args.binary).resolve()), str(Path(args.baseline).resolve())
variants = {'baseline': (baseline, '0'), 'table_off': (binary, '0'), 'table_on': (binary, '1')}
report = {'schema_version': 1, 'platform': platform.platform(), 'iterations': args.iterations,
          'default_enablement': False, 'runtime': {}, 'compile': {},
          'binary_bytes': {'baseline': Path(baseline).stat().st_size, 'current': Path(binary).stat().st_size}}

def run(name, source, mode='run'):
    executable, gate = variants[name]
    command = [executable, mode]
    if mode == 'bytecode': command.append('--metrics')
    else: command += ['--engine', 'bytecode']
    start = time.perf_counter_ns()
    result = subprocess.run([*command, str(source)], capture_output=True, timeout=60,
        env={**os.environ, 'HHY_BYTECODE_EXCEPTION_TABLES': gate, 'HHY_BYTECODE_CALL_PLANS': '0'})
    elapsed = time.perf_counter_ns() - start
    if result.returncode or result.stderr:
        raise RuntimeError((name, result.returncode, result.stderr.decode()))
    return elapsed, result.stdout

with tempfile.TemporaryDirectory(prefix='hhy-exception-evaluation-') as temp:
    for name, code, expected in (
        ('normal_try', 'let mut total = 0\nfor n in range(0, 100000) { try { total = total + n } catch err {} }\nprint(total)\n', b'4999950000\n'),
        ('caught_throw', 'let mut total = 0\nfor n in range(0, 10000) { try { throw(n) } catch err { total = total + err } }\nprint(total)\n', b'49995000\n'),
        ('attempt', 'let mut total = 0\nfor n in range(0, 10000) { let value = attempt { n + 1 }; total = total + value.value }\nprint(total)\n', b'50005000\n')):
        source = Path(temp) / (name + '.hhy'); source.write_text(code)
        for variant in variants:
            assert run(variant, source)[1] == expected
        rows = []
        for i in range(args.iterations):
            row = {}
            order = list(variants); order = order[i % 3:] + order[:i % 3]
            for variant in order:
                elapsed, output = run(variant, source)
                assert output == expected
                row[variant] = elapsed
            rows.append(row)
        report['runtime'][name] = {'source': code, 'samples_ns': rows,
            'median_on_vs_off_percent': statistics.median((r['table_on']/r['table_off']-1)*100 for r in rows),
            'median_off_vs_baseline_percent': statistics.median((r['table_off']/r['baseline']-1)*100 for r in rows)}
    source = Path(temp) / 'compile.hhy'
    source.write_text('\n'.join(f'fn f{i}(n) {{ try {{ return n + 1 }} catch err {{ return 0 }} }}' for i in range(200)))
    rows = []
    for variant in ('baseline', 'table_off'): run(variant, source, 'bytecode')
    for i in range(args.iterations):
        row = {}
        for variant in (('baseline', 'table_off') if i % 2 == 0 else ('table_off', 'baseline')):
            _, output = run(variant, source, 'bytecode')
            row[variant] = json.loads(output)
        rows.append(row)
    report['compile'] = {'function_count': 200, 'samples': rows,
        'median_compile_verify_change_percent': statistics.median(
            (r['table_off']['compile_verify_ns']/r['baseline']['compile_verify_ns']-1)*100 for r in rows),
        'median_verify_prepare_change_percent': statistics.median(
            (r['table_off']['verify_prepare_ns']/r['baseline']['verify_prepare_ns']-1)*100 for r in rows)}
output = Path(args.output); output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({name: {k:v for k,v in data.items() if k != 'samples_ns' and k != 'source'} for name,data in report['runtime'].items()}, indent=2))
print('compile/verify change:', report['compile']['median_compile_verify_change_percent'])
print('verify/prepare change:', report['compile']['median_verify_prepare_change_percent'])
