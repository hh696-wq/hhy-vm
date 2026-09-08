#!/usr/bin/env python3
"""Inline-cache semantics, bounded feedback, invalidation and stable target evidence."""
import itertools,json,os,subprocess,sys,tempfile
from pathlib import Path
binary=str(Path(sys.argv[1] if len(sys.argv)>1 else 'build/hhy').resolve())
base={**os.environ,'HHY_MAP_INLINE_CACHE':'0','HHY_PROFILE_LOOKUPS':'0','HHY_GC_STRESS':'0'}
count=0
with tempfile.TemporaryDirectory(prefix='hhy-inline-cache-') as directory:
    directory=Path(directory)
    overflow=directory/'overflow.hhy'
    overflow.write_text('let row = {value: 42}\n'+''.join('print(row.value)\n' for _ in range(80)))
    for source in ['tests/valid/inline-cache.hhy','tests/valid/closure-unwind-matrix.hhy','tests/valid/string-nul.hhy','tests/valid/call-unwind.hhy',str(overflow)]:
        oracle=subprocess.run([binary,'run','--engine','ast',source],env=base,capture_output=True,check=True,timeout=60)
        for engine,cache,feedback in itertools.product(('ast','bytecode'),('0','1'),('0','1')):
            report=directory/'profile.json'
            run=subprocess.run([binary,'profile','--engine',engine,'--heap','--format','json','--output',str(report),source],env={**base,'HHY_MAP_INLINE_CACHE':cache,'HHY_PROFILE_LOOKUPS':feedback},capture_output=True,timeout=60)
            assert (run.returncode,run.stdout,run.stderr)==(oracle.returncode,oracle.stdout,oracle.stderr),(source,engine,cache,feedback,run.stderr)
            profile=json.loads(report.read_text())['lookup_profile']
            assert profile['enabled']==(feedback=='1') and profile['map_cache_enabled']==(cache=='1'),profile
            if cache=='0' and feedback=='0': assert profile['reserved_bytes']==0
            else:
                m=profile['map']; domain=profile['domains'][2]
                assert m['cached_reads']+m['generic_reads']==domain['observations']
                assert m['guard_hit']+m['guard_miss']==domain['observations']
                assert len(domain['sites'])<=64
                for dom in profile['domains']:
                    assert sum(s['observations'] for s in dom['sites'])+dom['dropped']==dom['observations']
                    for site in dom['sites']: assert site['stable']+site['transitions']==site['observations']-1
                if source.endswith('inline-cache.hhy'):
                    assert m['slot_or_key_changed']>0 and m['key_missing']>0 and m['megamorphic_fallback']>0,m
                    if feedback=='1': assert any(s['distinct_capped']==2 for s in profile['domains'][1]['sites'])
                if source==str(overflow): assert domain['dropped']>0
            count+=1
print(f'inline cache: {count} differential/profile cases passed')
