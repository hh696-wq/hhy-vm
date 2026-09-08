#!/usr/bin/env python3
"""Paired local evidence for the experimental registered call-frame unwind path."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='build/hhy')
    parser.add_argument('--baseline-binary', required=True)
    parser.add_argument('--baseline-commit', help='commit used to build the supplied baseline, if known')
    parser.add_argument('--iterations', type=int, default=15)
    parser.add_argument('--output', default='build/benchmarks/call-unwind.json')
    args = parser.parse_args()
    if args.iterations < 3:
        parser.error('at least three measured pairs are required')
    binaries = {'baseline': str(Path(args.baseline_binary).resolve()),
                'off': str(Path(args.binary).resolve()), 'on': str(Path(args.binary).resolve())}
    sources = ['benchmarks/call-arguments.hhy', 'benchmarks/call-closure.hhy',
               'benchmarks/json-flow.hhy', 'benchmarks/core-flow.hhy']
    results = []
    with tempfile.TemporaryDirectory(prefix='hhy-call-unwind-bench-') as temp:
        profile_file = Path(temp) / 'profile.json'
        for source in sources:
            samples = {mode: [] for mode in binaries}
            compile_samples = {mode: [] for mode in ('baseline', 'off')}
            prepare_samples = {mode: [] for mode in ('baseline', 'off')}
            metrics = {}
            oracle = subprocess.run([binaries['baseline'], 'run', '--engine', 'ast', source], capture_output=True, check=True)
            for iteration in range(args.iterations + 2):
                order = list(binaries) if iteration % 2 == 0 else list(reversed(binaries))
                for mode in order:
                    env = {**os.environ, 'HHY_CALL_FRAME_UNWIND': '1' if mode == 'on' else '0', 'HHY_CALL_FRAME_POOL': 'legacy', 'HHY_BYTECODE_CALL_PLANS': '0', 'HHY_BYTECODE_EXCEPTION_TABLES': '0', 'HHY_PROFILE_DISPATCH': '0'}
                    started = time.perf_counter_ns()
                    run = subprocess.run([binaries[mode], 'run', '--engine', 'bytecode', source], env=env, capture_output=True, check=True)
                    elapsed = (time.perf_counter_ns() - started) / 1e6
                    if (run.stdout, run.stderr) != (oracle.stdout, oracle.stderr):
                        raise RuntimeError(f'semantic mismatch: {source}, {mode}')
                    if iteration >= 2:
                        samples[mode].append(elapsed)
                    if mode != 'on':
                        current = json.loads(subprocess.check_output([binaries[mode], 'bytecode', '--metrics', source], env=env))
                        metrics[mode] = current
                        if iteration >= 2:
                            compile_samples[mode].append(current['compile_verify_ns'])
                            prepare_samples[mode].append(current['verify_prepare_ns'])
            resources = {}
            for mode, binary in binaries.items():
                run = subprocess.run([binary, 'profile', '--engine', 'bytecode', '--heap', '--format', 'json', '--output', str(profile_file), source],
                                     env={**os.environ, 'HHY_CALL_FRAME_UNWIND': '1' if mode == 'on' else '0', 'HHY_CALL_FRAME_POOL': 'legacy', 'HHY_BYTECODE_CALL_PLANS': '0', 'HHY_BYTECODE_EXCEPTION_TABLES': '0', 'HHY_PROFILE_DISPATCH': '0'}, capture_output=True, check=True)
                if (run.stdout, run.stderr) != (oracle.stdout, oracle.stderr):
                    raise RuntimeError('profiling changed program output')
                resources[mode] = json.loads(profile_file.read_text())
            medians = {mode: statistics.median(values) for mode, values in samples.items()}
            results.append({'source': source, 'source_sha256': hashlib.sha256(Path(source).read_bytes()).hexdigest(),
                            'synthetic': '/call-' in source, 'wall_samples_ms': samples, 'wall_medians_ms': medians,
                            'on_off_ratio': medians['on'] / medians['off'],
                            'paired_on_off_percent': statistics.median((a/b-1)*100 for a,b in zip(samples['on'],samples['off'])),
                            'paired_off_baseline_percent': statistics.median((a/b-1)*100 for a,b in zip(samples['off'],samples['baseline'])),
                            'compile_verify_samples_ns': compile_samples, 'verify_prepare_samples_ns': prepare_samples,
                            'compile_metadata': metrics, 'resource_profiles': resources})
    report = {'schema_version': 1, 'platform': platform.platform(), 'machine': platform.machine(),
              'baseline_commit': args.baseline_commit,
              'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'working_tree_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'])),
              'iterations': args.iterations,
              'binaries': {mode: {'sha256': hashlib.sha256(Path(binary).read_bytes()).hexdigest(), 'bytes': Path(binary).stat().st_size} for mode, binary in binaries.items()},
              'workloads': results,
              'decision': {'default_enabled': False, 'status': 'experimental_only',
                           'reasons': ['single_platform_only', 'synthetic_call_workloads_do_not_satisfy_real_workload_gate'],
                           'tail_calls': 'not_eliminated', 'unwind_table': 'verified_runtime_call_state_v1'}}
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Call-unwind evidence saved to {output}; default remains off')


if __name__ == '__main__':
    main()
