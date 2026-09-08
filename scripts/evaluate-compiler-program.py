#!/usr/bin/env python3
"""Reproducible whole-program compiler budgets; no cross-platform extrapolation."""
import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import statistics
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='build/hhy')
    parser.add_argument('--probe', default='build/hhy-resource-probe')
    parser.add_argument('--iterations', type=int, default=15)
    parser.add_argument('--output', default='build/benchmarks/compiler-program.json')
    args = parser.parse_args()
    if args.iterations < 3:
        parser.error('at least three measured pairs required')
    env = {k: v for k, v in os.environ.items() if not k.startswith('HHY_COMPILER')}
    rows = []
    with tempfile.TemporaryDirectory(prefix='hhy-compiler-budget-') as directory:
        directory = Path(directory)
        hot = directory/'constant-loop.hhy'
        hot.write_text('let mut i = 0\nlet mut result = 0\nwhile i < 200000 {\nresult = result + (10 + 20) * (4 + 6)\ni = i + 1\n}\nprint(result)\n')
        propagation = directory/'propagation.hhy'
        propagation.write_text('fn value(x) { let a = 20\nlet b = a\n1 + 2\nreturn x + b * 2\nprint(99) }\nlet mut i = 0\nlet mut result = 0\nwhile i < 50000 { result = value(i)\ni = i + 1 }\nprint(result)\n')
        for source in [hot, propagation, Path('benchmarks/core-flow.hhy'),
                       Path('benchmarks/json-flow.hhy'), Path('benchmarks/call-closure.hhy')]:
            samples = {mode: [] for mode in ('direct', 'ir-off', 'ir')}
            oracle = subprocess.run([args.binary, 'run', '--engine', 'bytecode', str(source)], env=env,
                                    capture_output=True, timeout=60)
            assert oracle.returncode == 0, oracle.stderr
            for iteration in range(args.iterations + 2):
                modes = list(samples)
                if iteration % 2:
                    modes.reverse()
                for mode in modes:
                    current = {**env, 'HHY_COMPILER': 'bc' if mode == 'direct' else 'ir',
                               'HHY_COMPILER_DISABLE': 'all' if mode == 'ir-off' else ''}
                    compiled = subprocess.run([args.binary, 'bytecode', '--metrics', str(source)],
                                              env=current, capture_output=True, check=True, timeout=60)
                    metrics = json.loads(compiled.stdout)
                    report = directory/'resource.json'
                    run = subprocess.run([args.probe, str(report), 'bytecode', str(source)],
                                         env=current, capture_output=True, timeout=60)
                    assert (run.returncode, run.stdout, run.stderr) == (oracle.returncode, oracle.stdout, oracle.stderr), (source, mode, run.stderr)
                    resource = json.loads(report.read_text())
                    if iteration >= 2:
                        samples[mode].append({'compile_verify_ns': metrics['compile_verify_ns'],
                            'instructions': metrics['instructions'], 'constants': metrics['constants'],
                            'runtime_including_prepare_ns': resource['elapsed_ns'],
                            'allocated_bytes': resource['allocated_bytes'],
                            'heap_bytes': resource['heap_bytes'], 'max_rss_native_units': resource['max_rss_native_units']})
            medians = {mode: {key: statistics.median(s[key] for s in values) for key in values[0]}
                       for mode, values in samples.items()}
            direct, optimized = medians['direct'], medians['ir']
            budget = {
                'compile': optimized['compile_verify_ns'] <= 2 * direct['compile_verify_ns'] + 500000,
                'instructions': optimized['instructions'] <= direct['instructions'],
                'runtime_benefit': optimized['runtime_including_prepare_ns'] <= .95 * direct['runtime_including_prepare_ns'],
                'allocation': optimized['allocated_bytes'] <= 1.01 * direct['allocated_bytes'],
            }
            ir_report = json.loads(subprocess.check_output(['build/hhy-compiler-probe', '--metrics', str(source)], env=env))
            rows.append({'source': source.name, 'source_text': source.read_text(),
                         'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                         'samples': samples, 'medians': medians, 'budget_pass': budget,
                         'runtime_ratio': optimized['runtime_including_prepare_ns'] / direct['runtime_including_prepare_ns'],
                         'compiler_report': ir_report})
    result = {'schema_version': 1, 'platform': platform.platform(), 'machine': platform.machine(),
        'iterations': args.iterations, 'warmups': 2, 'binary_sha256': hashlib.sha256(Path(args.binary).read_bytes()).hexdigest(),
        'probe_sha256': hashlib.sha256(Path(args.probe).read_bytes()).hexdigest(),
        'budgets': {'compile_ns': '2 * direct + 500000', 'instruction_ratio_max': 1.0,
                    'runtime_ratio_max': .95, 'allocation_ratio_max': 1.01},
        'rows': rows, 'default_admitted': False, 'cross_platform_admitted': False,
        'notes': ['Runtime interval includes compiler preparation, not a precompiled execution-only timer.',
                  'Synthetic constant and propagation cases are not evidence of broad real-project speedup.',
                  'GC heap/RSS are observational; managed quotas are separately checked by differential tests.',
                  'Every platform must satisfy its own budgets before any default admission.']}
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    for row in rows:
        print(row['source'], row['runtime_ratio'], row['budget_pass'])
    print('Compiler budget evidence saved to', output)

if __name__ == '__main__':
    main()
