#!/usr/bin/env python3
"""Seeded expression differential corpus and refusal of non-prototype semantics."""
import json,random,subprocess,sys,tempfile
from pathlib import Path
probe=sys.argv[1] if len(sys.argv)>1 else 'build/hhy-ir-probe'
rng=random.Random(1700)
def expr(depth):
    if not depth or rng.random()<.3:return str(rng.randint(0,100))
    return '('+expr(depth-1)+rng.choice([' + ',' - ',' * ',' % '])+expr(depth-1)+')'
expressions=[expr(4) for _ in range(120)]+['9223372036854775807 + 1','0 - 9223372036854775807 - 2','(0 - 9223372036854775807 - 1) % -1','-(0 - 9223372036854775807 - 1)','1 % 0','(9223372036854775807 + 1) % 0','(1 % 0) + (9223372036854775807 + 1)']
with tempfile.TemporaryDirectory(prefix='hhy-ir-') as directory:
    source=Path(directory)/'expression.hhy'
    for expression in expressions:
        source.write_text('print('+expression+')\n')
        reports=[]
        for flag in ('--no-fold','--fold'):
            report=json.loads(subprocess.check_output([probe,flag,str(source)],timeout=10));assert report['verified'],expression;reports.append(report)
        before,after=reports
        assert before['result']['ok']==after['result']['ok'] and before['result']['value']==after['result']['value'] and before['result']['reason']==after['result']['reason'],expression
        if not after['result']['ok']:assert before['before']==after['after'],expression
        for engine in ('ast','bytecode'):
            run=subprocess.run(['build/hhy','run','--engine',engine,str(source)],capture_output=True,text=True,timeout=10)
            value=after['result']
            if value['ok']:assert run.returncode==0 and run.stdout==str(value['value'])+'\n' and not run.stderr,(expression,run.stderr)
            else:
                message={'division_by_zero':'division by zero','integer_negation_overflow':'Int negation overflow','integer_overflow':'Int arithmetic overflow'}[value['reason']]
                instruction=after['after']['instructions'][value['instruction']]
                assert run.returncode==1 and not run.stdout and message in run.stderr,(expression,run.stderr,value)
                assert ':'+str(instruction['line'])+':'+str(instruction['column'])+':' in run.stderr,(expression,run.stderr,instruction)
    for expression in ('1 / 2','1.0 + 2.0','true','"a" + "b"','length([1])','1_000 + 2'):
        source.write_text('print('+expression+')\n')
        report=json.loads(subprocess.check_output([probe,'--fold',str(source)],timeout=10));assert not report['supported'],expression
    source.write_text('let value = 1\nprint(value + 2)\n')
    assert not json.loads(subprocess.check_output([probe,'--fold',str(source)]))['supported']
print(f'IR: {len(expressions)} seeded/boundary expressions across fold off/on and AST/Bytecode; 7 unsupported cases refused')
