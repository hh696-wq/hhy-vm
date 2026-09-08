#!/usr/bin/env python3
"""Exercise hot guards, deopt operands, scalar escape barriers and allocation parity."""
import json
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1] if len(sys.argv)>1 else 'build/hhy').resolve())
base = dict(os.environ)
for key in ('HHY_FEEDBACK_SPECIALIZATION', 'HHY_SCALAR_REPLACEMENT', 'HHY_COMPILER'):
    base.pop(key, None)
count = 0
with tempfile.TemporaryDirectory(prefix='hhy-typed-') as directory:
    source = Path(directory)/'case.hhy'
    report = Path(directory)/'profile.json'
    def run(text, enabled, scalar=True, compiler='bc', profile=False, extra=None, options=()):
        global count
        count += 1
        source.write_text(text)
        env = {**base, 'HHY_FEEDBACK_SPECIALIZATION':str(int(enabled)),
               'HHY_SCALAR_REPLACEMENT':str(int(scalar)), 'HHY_COMPILER':compiler, **(extra or {})}
        command = [binary, 'profile', '--heap', '--format', 'json', '--output', str(report)] if profile else [binary, 'run']
        result = subprocess.run([*command, *options, str(source)], env=env, capture_output=True, timeout=30)
        return (result.returncode, result.stdout, result.stderr), json.loads(report.read_text()) if profile else None
    def compare(text):
        oracle,_ = run(text,False)
        for compiler in ('bc','ir'):
            for scalar in (False,True):
                got,_ = run(text,True,scalar,compiler)
                assert got == oracle,(text,compiler,scalar,oracle,got)
    warm = 'for i in 0..16 { f(i) }\n'
    cases = [
        ('x + 1', 'print(f(1.5))\nprint(f(100))'),
        ('x * x', 'print(f(9223372036854775807))'),
        ('x + 1', 'print(f(9223372036854775807))'),
        ('-x', 'print(f(0 - 9223372036854775807 - 1))'),
        ('x % (x - 17)', 'print(f(17))'),
        ('x > 9007199254740992', 'print(f(9007199254740993))'),
        ('[x + 1, x * 2][0]', 'print(f(40))'),
        ('[x + 1, x * x][0]', 'print(f(4000000000))'),
        ('[x < 3, x > 4][0]', 'print(f(100))'),
        ('[x + 1, x * 2]', 'print(f(40))'),
        ('[x][x]', ''),
        ('x + captured', 'print(f(100))'),
        ('[x][0] + [x * 2][0]', 'print(f(100))'),
    ]
    for expr, tail in cases:
        compare('let captured = 3\nfn f(x) { return '+expr+' }\n'+warm+tail+'\n')
    compare('let f = { x -> return [x + 1, x * 2][1] }\n'+warm+'print(f(90))\n')
    compare('let f = { x -> [x + 1, x * 2][1] }\n'+warm+'print(f(90))\n')
    compare('fn f(x) { return x + 1 }\n'+warm+'for i in 0..20 { print(f(1.5))\nprint(f(i)) }\n')
    compare('fn add(x, y) { return x + y }\nfor i in 0..20 { print(add(i, 4)) }\nprint(add(1.5, 3))\n')
    compare('fn a(x) { return x + 1 }\nfn b(x) { return x * 2 }\nlet mut f = a\n'+warm+'f = b\n'+warm+'print(f(3))\n')
    for extra in ({'HHY_GC_STRESS':'1'}, {'HHY_BYTECODE_CALL_PLANS':'1','HHY_BYTECODE_EXCEPTION_TABLES':'1','HHY_CALL_FRAME_POOL':'bounded','HHY_CALL_FRAME_UNWIND':'1','HHY_MAP_INLINE_CACHE':'1'}):
        text='fn f(x) { return [x + 1, x * 2][0] }\n'+warm+'print(f(40))\n'
        for limit in ('1000','10000','100000','1000000'):
            oracle,_=run(text,False,extra=extra,options=('--limit','max_memory='+limit))
            got,_=run(text,True,extra=extra,options=('--limit','max_memory='+limit))
            assert got==oracle,(extra,limit,oracle,got)
    rng = random.Random(1704)
    for _ in range(35):
        expr = '((x '+rng.choice(['+','-','*'])+' '+str(rng.randrange(1,20))+') % '+str(rng.randrange(1,20))+')'
        compare('fn f(x) { return '+expr+' }\nfor i in 0..24 { print(f(i)) }\n')
    text='fn f(x) { return [x + 1, x * 2][0] }\n'+warm+'print(f(1.5))\n'
    oracle,p0=run(text,False,profile=True)
    got,p1=run(text,True,profile=True)
    assert got==oracle
    for field in ('allocations','allocated_bytes'):
        assert p0[field]==p1[field],(field,p0[field],p1[field])
    sites=p1['typed_specialization']['sites']
    assert any(s['hits']>=9 and s['guard_deopts']==1 and s['scalar_reservations']>=9 for s in sites),sites
    text='fn f(x) { return x * x }\n'+warm+'print(f(9223372036854775807))\n'
    _,p=run(text,True,profile=True)
    assert sum(s['arithmetic_deopts'] for s in p['typed_specialization']['sites'])==1
    text='fn f(x) { return x + 1 }\n'+warm+'for i in 0..10 { f(1.5) }\nprint(f(3))\n'
    _,p=run(text,True,profile=True)
    assert any(s['disabled'] and s['guard_deopts']==10 for s in p['typed_specialization']['sites'])
print(f'typed specialization: {count} execution paths passed')
