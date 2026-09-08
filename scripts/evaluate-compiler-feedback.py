#!/usr/bin/env python3
"""Audit observed call feedback and allocation escape candidates before admission."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='build/hhy')
    parser.add_argument('--output', default='build/benchmarks/compiler-feedback.json')
    args = parser.parse_args()
    cases = {
        'stable': 'fn id(x) { return x }\nlet mut i = 0\nwhile i < 10000 { id(i)\ni = i + 1 }\nprint(i)\n',
        'changing': 'fn a(x) { return x }\nfn b(x) { return x + 1 }\nlet mut f = a\nlet mut i = 0\nwhile i < 10000 { if i % 2 == 0 { f = a } else { f = b }\nf(i)\ni = i + 1 }\nprint(i)\n',
        'allocations': 'fn id(x) { return x }\nlet a = [1, 2]\nprint(a)\nprint([3, 4][0])\nprint(id({value: 5}))\n',
    }
    rows = []
    with tempfile.TemporaryDirectory(prefix='hhy-compiler-feedback-') as directory:
        directory = Path(directory)
        for name, text in cases.items():
            source, output = directory/(name + '.hhy'), directory/(name + '.json')
            source.write_text(text)
            env = {k: v for k, v in os.environ.items() if not k.startswith('HHY_COMPILER')}
            run = subprocess.run([args.binary, 'profile', '--engine', 'bytecode', '--format', 'json',
                '--output', str(output), str(source)], capture_output=True, timeout=60,
                env={**env, 'HHY_COMPILER': 'ir', 'HHY_PROFILE_LOOKUPS': '1'})
            assert run.returncode == 0, run.stderr
            profile = json.loads(output.read_text())['lookup_profile']
            target = next(domain for domain in profile['domains'] if domain['kind'] == 'call_target')
            compiler = json.loads(subprocess.check_output(['build/hhy-compiler-probe', '--metrics', str(source)], env=env))
            rows.append({'case': name, 'source_text': text, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                'feedback': target, 'compiler_analysis': compiler['analysis'],
                'specialization_decision': {'admitted': False,
                    'stable_observed_sites': sum(s['distinct_capped'] == 1 and s['observations'] >= 1000 for s in target['sites']),
                    'changing_observed_sites': sum(s['distinct_capped'] > 1 for s in target['sites']),
                    'reason': 'target_identity_observed_but_argument_types_guard_lifetimes_and_cross_platform_benefits_not_established'},
                'escape_decision': {'admitted': False,
                    'reason': 'even_local_aggregate_candidates_have_observable_allocation_quota_and_error_behavior;_retain_managed_storage'}})
    result = {'schema_version': 1, 'platform': platform.platform(), 'machine': platform.machine(), 'rows': rows,
              'default_admitted': False, 'type_feedback_available': False,
              'specialization_implementation': 'retain_generic_runtime_no_new_speculative_code',
              'escape_implementation': 'conservative_immediate_use_analysis_no_allocation_elision',
              'not_a_no_benefit_claim': True}
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print('Feedback and escape admission decisions saved to', output)

if __name__ == '__main__':
    main()
