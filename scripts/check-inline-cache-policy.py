#!/usr/bin/env python3
"""Keep conditional decisions separate from an unsupported no-benefit claim."""
import json,os,subprocess,sys,tempfile
from pathlib import Path
policy=json.loads(Path('benchmarks/vm-inline-cache-policy.json').read_text())
assert policy['stage']=='v1.6.2'
for key in ('map_cache_default','lookup_feedback_default','cached_managed_pointers','cross_platform_admission','universal_no_benefit_claim'): assert policy[key] is False
assert policy['sites_per_domain']==64 and policy['targets_per_site']==4 and policy['map_guard_failure_limit']==4
binary=sys.argv[1] if len(sys.argv)>1 else 'build/hhy'
env={k:v for k,v in os.environ.items() if k not in ('HHY_MAP_INLINE_CACHE','HHY_PROFILE_LOOKUPS')}
with tempfile.TemporaryDirectory() as temp:
    output=str(Path(temp)/'profile.json')
    subprocess.run([binary,'profile','--format','json','--output',output,'tests/valid/inline-cache.hhy'],env=env,check=True,capture_output=True)
    p=json.loads(Path(output).read_text())['lookup_profile']
    assert p['enabled'] is False and p['map_cache_enabled'] is False and p['reserved_bytes']==0
print('inline-cache policy and actual default-off/no-allocation gate passed')
