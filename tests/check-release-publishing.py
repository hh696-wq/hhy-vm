#!/usr/bin/env python3
"""Execute actual publishing steps against local tags and a non-writing gh stub."""
import os
from pathlib import Path
import subprocess
import tempfile

workflow = Path('.github/workflows/release.yml').read_text()
def step(name):
    text=workflow.split('      - name: '+name+'\n',1)[1].split('        run: |\n',1)[1]
    lines=[]
    for line in text.splitlines():
        if line and not line.startswith('          '):break
        lines.append(line[10:])
    return '\n'.join(lines)
core=step('Create or update release with every asset')
database=step('Publish independently versioned database package')
with tempfile.TemporaryDirectory(prefix='hhy-release-identity-') as d:
    root=Path(d);repo=root/'repo';repo.mkdir();bindir=root/'bin';bindir.mkdir();log=root/'gh.log'
    def git(*args):
        return subprocess.check_output(['git',*args],cwd=repo,stderr=subprocess.DEVNULL,text=True).strip()
    git('init','-q');git('config','user.name','HHY release test');git('config','user.email','test@example.invalid')
    git('config','core.hooksPath','/dev/null')
    for name,value in {'VERSION':'1.7.0\n','src/main.c':'runtime\n','extensions/database/hhy.toml':'[package]\nversion = "1.0.0"\n','extensions/database/main.c':'database\n'}.items():
        p=repo/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(value)
    git('add','.');git('commit','-qm','published payload');git('tag','v1.7.0');git('tag','database-v1.0.0')
    git('remote','add','origin',str(repo))
    (repo/'ci-marker').write_text('independent runtime release\n');git('add','.');git('commit','-qm','new host commit')
    assert git('rev-parse','HEAD')!=git('rev-parse','database-v1.0.0')
    stub=bindir/'gh';stub.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$TEST_GH_LOG"\ncase "$*" in\n  "release view "*) printf "false\\n"; exit 0;;\n  *) echo "unexpected remote mutation: $*" >&2; exit 99;;\nesac\n');stub.chmod(0o755)
    env={**os.environ,'PATH':str(bindir)+os.pathsep+os.environ['PATH'],'TEST_GH_LOG':str(log),
         'RELEASE_TAG':'v1.7.0','GITHUB_SHA':git('rev-parse','HEAD'),'GITHUB_EVENT_NAME':'workflow_dispatch'}
    def check(script,expected):
        log.write_text('')
        r=subprocess.run(['bash','-e','-o','pipefail','-c',script],cwd=repo,env=env,capture_output=True,text=True)
        assert (r.returncode==0)==expected,(r.returncode,r.stdout,r.stderr)
        assert all(line.startswith('release view ') for line in log.read_text().splitlines()),log.read_text()
    check(core,True);check(database,True)
    (repo/'src/main.c').write_text('changed runtime\n');git('add','.');git('commit','-qm','runtime change')
    check(core,False);check(database,True)
    (repo/'extensions/database/main.c').write_text('changed database\n');git('add','.');git('commit','-qm','database change')
    check(database,False)
print('release publishing: unchanged versions retained; changed implementations rejected; no remote mutation')
