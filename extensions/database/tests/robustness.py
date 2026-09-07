#!/usr/bin/env python3
"""Protocol, cancellation, byte budgets, TLS, and sustained pool checks."""
import argparse,json,time,subprocess,os
from pathlib import Path
from integration import Client

def check(c,cfg,ca):
    bad=dict(cfg,allow=['not-the-endpoint:1'])
    c.call('ping',bad,error='DB_PERMISSION')
    c.call('ping',dict(cfg,unexpected=True),error='DB_CONFIG')
    c.call('ping',dict(cfg,max_open=0),error='DB_CONFIG')
    if ca:
        secure=dict(cfg,tls='verify_identity',ca=str(ca.resolve()))
        c.call('ping',secure)
        c.seq+=1;c.send(dict(type='call',request_id=str(c.seq),callable='database.ping',scope='tls',arguments=[dict(secure,host='localhost',allow=['localhost:'+str(cfg['port'])])]))
        r=c.read();assert r['type']=='error' and r['code'] in ('DB_TLS','DB_CONNECT'),r
        c.seq+=1;c.send(dict(type='call',request_id=str(c.seq),callable='database.ping',scope='tls',arguments=[dict(secure,ca='/nonexistent/hhy-test-ca.pem')]))
        r=c.read();assert r['type']=='error' and r['code'] in ('DB_TLS','DB_CONNECT'),r
    my=cfg['driver']=='mysql';slow='SELECT SLEEP(10)' if my else 'SELECT pg_sleep(10)'
    c.seq+=1;request=str(c.seq);start=time.monotonic()
    c.send(dict(type='call',request_id=request,callable='database.query',arguments=[cfg,slow,[]],scope='cancel'))
    time.sleep(.15);c.send(dict(type='cancel',request_id='cancel',target=request))
    r=c.read();assert r['request_id']==request and r['code']=='DB_CANCELLED',r
    assert time.monotonic()-start<3
    c.call('ping',cfg)
    c.call('query',cfg,"SELECT REPEAT('x', 70000) AS too_large",[],error='DB_LIMIT')
    c.call('ping',cfg)
    tx=c.call('begin',cfg)
    c.call('execute',tx,'COMMIT',[],error='DB_TRANSACTION_SQL')
    c.call('execute',tx,'CREATE TABLE forbidden_inside_tx (id INT)',[],error='DB_TRANSACTION_SQL')
    c.call('rollback',tx)
    # Remote-host policy is explicit even when the address happens to resolve locally.
    remote=f"{cfg['driver']}://{cfg['user']}:unused@db.example.invalid:{cfg['port']}/{cfg['database']}"
    c.call('ping',remote,error='DB_PERMISSION')
    short=dict(cfg,timeout_ms=100)
    handle=c.call('begin',short);time.sleep(.15)
    c.call('query',handle,'SELECT 1',[],error='DB_RESOURCE')
    c.call('ping',cfg)
    # Expiring handles cannot leak all 64 native slots.
    assert c.call('stats')['pinned']==0

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--binary',type=Path,default=Path('extensions/database/bin/hhy-database'));ap.add_argument('--config',type=Path,required=True);ap.add_argument('--ca',type=Path);ap.add_argument('--soak-seconds',type=int,default=0);a=ap.parse_args()
    for cfg in json.loads(a.config.read_text()):
        c=Client(a.binary.resolve())
        try:
            check(c,cfg,a.ca)
            until=time.monotonic()+a.soak_seconds;iterations=0
            baseline=c.call('stats')['created']
            while time.monotonic()<until:
                assert c.call('query',cfg,'SELECT 1 AS value',[])['rows']==[{'value':'1'}]
                iterations+=1
                if iterations%100==0:
                    cur=c.call('cursor',cfg,'SELECT 2 AS value',[])
                    c.call('fetch',cur);c.call('close',cur)
                    stats=c.call('stats');assert stats['pinned']==0 and stats['open']<=8,stats
            print(json.dumps({'driver':cfg['driver'],'robustness':'passed','soak_seconds':a.soak_seconds,'iterations':iterations,'stats':c.call('stats')}))
        finally:c.close()
if __name__=='__main__':main()
