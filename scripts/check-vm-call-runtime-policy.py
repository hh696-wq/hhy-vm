#!/usr/bin/env python3
"""Keep defaults and recursion diagnostics aligned with the stage decision."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else 'build/hhy').resolve())
policy = json.loads(Path('benchmarks/vm-call-runtime-policy.json').read_text())
assert policy['schema_version'] == 1 and policy['stage'] == 'v1.6.1'
assert policy['tail_calls'] == 'preserve_frames'
assert policy['closure_capture'] == 'shared_lexical_environment'
assert policy['default_promotion'] == 'not_admitted' and not policy['cross_platform_performance_admission']
assert policy['defaults'] == {'HHY_BYTECODE_CALL_PLANS':'0', 'HHY_BYTECODE_EXCEPTION_TABLES':'0',
                              'HHY_CALL_FRAME_POOL':'legacy', 'HHY_CALL_FRAME_UNWIND':'0'}
env = dict(os.environ)
for key in policy['defaults']: env.pop(key, None)
with tempfile.TemporaryDirectory(prefix='hhy-call-policy-') as temp:
    report = Path(temp)/'profile.json'
    run = subprocess.run([binary,'profile','--engine','bytecode','--format','json','--output',str(report),
        'tests/valid/closure-unwind-matrix.hhy'], env=env, capture_output=True, timeout=60)
    assert run.returncode == 0 and run.stdout == b'12\n12\n103\n12\ntrue\ntrue\n42\n[8, 9, 10]\n', run.stderr
    data = json.loads(report.read_text())
    assert data['call_layout']['selected_calls'] == 0 and data['call_layout']['generic_calls'] > 0
    assert data['exception_layout']['selected_regions'] == 0 and data['exception_layout']['generic_regions'] > 0
    assert not data['frame_pool']['bounded'] and not data['call_unwind']['enabled']
    assert data['call_unwind']['table_version'] == policy['call_unwind_version']
    for engine in ('ast','bytecode'):
        run = subprocess.run([binary,'run','--engine',engine,'--limit','max_recursion=8',
            'tests/invalid-runtime/recursion-limit.hhy'], env=env, capture_output=True, timeout=60)
        assert run.returncode == 1 and b'maximum call depth exceeded' in run.stderr, run.stderr
print('v1.6.1 policy: experimental defaults, shared captures and preserved tail recursion limits passed')
