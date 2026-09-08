#!/usr/bin/env python3
"""Paired typed MIR budgets. Admission is per workload and platform, never inferred."""
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
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',default='build/hhy')
p.add_argument('--iterations',type=int,default=15)
p.add_argument('--output',default='build/benchmarks/typed-specialization.json')
a=p.parse_args()
assert a.iterations>=3
base={k:v for k,v in os.environ.items() if not k.startswith(('HHY_COMPILER','HHY_FEEDBACK_SPECIALIZATION','HHY_SCALAR_REPLACEMENT'))}
rows=[]
with tempfile.TemporaryDirectory(prefix='hhy-typed-budget-') as directory:
    directory=Path(directory)
    arithmetic=directory/'typed-arithmetic.hhy'
    arithmetic.write_text('fn f(x) { return (x + 1) * 3 % 10007 }\nlet mut r = 0\nfor i in 0..100000 { r = f(i) }\nprint(r)\n')
    scalar=directory/'typed-scalar.hhy'
    scalar.write_text('fn f(x) { return [x + 1, x * 2, x % 7][1] }\nlet mut r = 0\nfor i in 0..100000 { r = f(i) }\nprint(r)\n')
    for source in (arithmetic,scalar,Path('benchmarks/core-flow.hhy'),Path('benchmarks/json-flow.hhy'),Path('benchmarks/call-closure.hhy')):
        samples={mode:[] for mode in ('off','on')}
        oracle=None
        for iteration in range(a.iterations+2):
            for mode in (('off','on') if iteration%2==0 else ('on','off')):
                env={**base,'HHY_COMPILER':'ir','HHY_FEEDBACK_SPECIALIZATION':str(int(mode=='on')),'HHY_SCALAR_REPLACEMENT':'1'}
                metrics=json.loads(subprocess.check_output([a.binary,'bytecode','--metrics',str(source)],env=env))
                start=time.perf_counter_ns()
                r=subprocess.run([a.binary,'run',str(source)],env=env,capture_output=True,timeout=90)
                elapsed=time.perf_counter_ns()-start
                observed=(r.returncode,r.stdout,r.stderr)
                if oracle is None: oracle=observed
                assert observed==oracle and r.returncode==0,(source,mode,observed,oracle)
                if iteration>=2:samples[mode].append({'elapsed_ns':elapsed,'compile_ns':metrics['compile_verify_ns'],'typed_plan_bytes':metrics['typed_plan_bytes'],'instructions':metrics['instructions']})
        profiles={}
        for mode in samples:
            env={**base,'HHY_COMPILER':'ir','HHY_FEEDBACK_SPECIALIZATION':str(int(mode=='on')),'HHY_SCALAR_REPLACEMENT':'1'}
            report=directory/'profile.json'
            r=subprocess.run([a.binary,'profile','--heap','--format','json','--output',str(report),str(source)],env=env,capture_output=True,timeout=90)
            assert (r.returncode,r.stdout,r.stderr)==oracle
            profiles[mode]=json.loads(report.read_text())
        median={mode:{key:statistics.median(s[key] for s in values) for key in values[0]} for mode,values in samples.items()}
        off,on=median['off'],median['on']
        gates={'compile':on['compile_ns']<=2*off['compile_ns']+500000,'instructions':on['instructions']<=off['instructions'],
               'runtime':on['elapsed_ns']<=.95*off['elapsed_ns'],'managed_allocation':profiles['on']['allocated_bytes']<=1.01*profiles['off']['allocated_bytes'],
               'typed_storage_cap':on['typed_plan_bytes']<=64*1568}
        row={'source':source.name,'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'samples':samples,'medians':median,'gates':gates,'admitted':all(gates.values()),'runtime_ratio':on['elapsed_ns']/off['elapsed_ns'],'profiles':profiles}
        rows.append(row)
        print(source.name,row['runtime_ratio'],gates,flush=True)
result={'schema_version':1,'platform':platform.platform(),'machine':platform.machine(),'binary_sha256':hashlib.sha256(Path(a.binary).read_bytes()).hexdigest(),'iterations':a.iterations,'warmups':2,'rows':rows,'default_admitted':False,'notes':['Includes process startup and compilation.','Scalar replacement retains allocation quota reservations; no heap reduction claimed.','Default admission requires real workloads to pass independently on every platform.']}
output=Path(a.output);output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,indent=2)+'\n')
