#!/usr/bin/env python3
"""Offline protocol boundary checks: no database or credentials required."""
import json,subprocess
from pathlib import Path
from integration import Client
binary=Path('extensions/database/bin/hhy-database').resolve()
c=Client(binary)
try:
    assert c.call('capabilities')['version'].startswith('1.0.0')
    for value in [None,[],{},42,False,'invalid://database',{'host':'127.0.0.1','port':0},{'host':'127.0.0.1','port':'3306'}]:
        c.seq+=1;c.send(dict(type='call',request_id=str(c.seq),callable='database.ping',scope='offline',arguments=[value]))
        r=c.read();assert r['type']=='error',r
        assert 'password' not in r.get('message','').lower(),r
    c.call('query','a'*64,'SELECT 1',[],error='DB_RESOURCE')
    c.call('unknown',{},error='DB_ARGUMENT')
    c.call('stats',1,error='DB_ARGUMENT')
    assert c.call('stats')['open']==0
finally:c.close()
for invalid in [b'[]\n',b'{"x":1,"x":2}\n',b'x'* (1024*1024+4)+b'\n',b'{"request_id":"bad","extension_id":"other","protocol_version":"1.0"}\n']:
    p=subprocess.run([str(binary),'--protocol','1'],input=invalid,capture_output=True,timeout=5)
    assert p.returncode==2,p
print('Protocol identity, invalid JSON, duplicate keys, message size, configuration, and stale resources passed')
