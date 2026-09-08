#!/usr/bin/env python3
"""Local, paired dispatch evidence; frequency is never proof of fusion safety."""
import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import statistics
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='build/hhy')
    parser.add_argument('--baseline-binary', help='optional pre-change binary for paired regression evidence')
    parser.add_argument('--iterations', type=int, default=9)
    parser.add_argument('--output', default='build/benchmarks/vm-dispatch.json')
    args = parser.parse_args()
    if args.iterations < 3:
        parser.error('at least three paired iterations are required')
    binary = str(Path(args.binary).resolve())
    workloads = ['benchmarks/opcode-loop.hhy', 'benchmarks/core-flow.hhy',
                 'benchmarks/json-flow.hhy', 'tests/valid/bytecode-specialization-profile.hhy']
    results = []
    with tempfile.TemporaryDirectory(prefix='hhy-dispatch-evidence-') as temp:
        profile_path = Path(temp) / 'profile.json'
        for source in workloads:
            samples = {key: [] for key in ('ordinary', 'profile', 'dispatch')}
            if args.baseline_binary:
                samples['baseline'] = []
            oracle = subprocess.run([binary, 'run', '--engine', 'ast', source], capture_output=True, check=True)
            last = None
            for iteration in range(args.iterations + 2):
                order = list(samples) if iteration % 2 == 0 else list(reversed(samples))
                for mode in order:
                    selected_binary = str(Path(args.baseline_binary).resolve()) if mode == 'baseline' else binary
                    command = [selected_binary, 'run', '--engine', 'bytecode', source] if mode in ('ordinary', 'baseline') else [
                        binary, 'profile', '--engine', 'bytecode', '--cpu', '--heap', '--format', 'json',
                        '--output', str(profile_path), source]
                    started = time.perf_counter_ns()
                    run = subprocess.run(command, env={**os.environ, 'HHY_PROFILE_DISPATCH': '1' if mode == 'dispatch' else '0'}, capture_output=True, check=True)
                    elapsed = (time.perf_counter_ns() - started) / 1e6
                    if run.stdout != oracle.stdout or run.stderr != oracle.stderr:
                        raise RuntimeError(f'semantic differential: {source}, {mode}')
                    if iteration >= 2:
                        samples[mode].append(elapsed)
                    if mode == 'dispatch':
                        last = json.loads(profile_path.read_text())
            if last['dispatch_profile']['status'] != 'enabled':
                raise RuntimeError('dispatch profiling could not be enabled')
            medians = {key: statistics.median(value) for key, value in samples.items()}
            sequences = last['dispatch_profile']['sequences']
            ranked = sorted((x for x in sequences if len(x['opcodes']) > 1), key=lambda x: (-x['count'], x['opcodes']))
            results.append({'source': source, 'source_sha256': hashlib.sha256(Path(source).read_bytes()).hexdigest(),
                            'samples_ms': samples, 'medians_ms': medians,
                            'dispatch_instrumentation_ratio': medians['dispatch'] / medians['profile'],
                            'profile': last, 'ranked_sequences': ranked,
                            'dispatch_only_cpu_seconds': None,
                            'cost_reason': 'recursive evaluator timings include semantic work; switch-only cost is not isolated'})
    report = {'schema_version': 1, 'platform': platform.platform(), 'machine': platform.machine(),
              'baseline_binary_sha256': hashlib.sha256(Path(args.baseline_binary).read_bytes()).hexdigest() if args.baseline_binary else None,
              'baseline_binary_bytes': Path(args.baseline_binary).stat().st_size if args.baseline_binary else None,
              'binary_bytes': Path(binary).stat().st_size,
              'binary_sha256': hashlib.sha256(Path(binary).read_bytes()).hexdigest(),
              'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'working_tree_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'])),
              'iterations': args.iterations, 'workloads': results,
              'decision': {'superinstructions': 'not_admitted', 'inline_cache': 'not_evaluated',
                           'gc_scheduler': 'not_evaluated', 'optimizing_compiler': 'blocked_by_evidence_gate',
                           'reasons': ['single_platform_only', 'dispatch_cost_not_isolated',
                                       'recursive_transitions_are_not_fusible_adjacency', 'no_optimized_candidate_measured']}}
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Local dispatch evidence written to {output}; no optimization admitted')


if __name__ == '__main__':
    main()
