#!/usr/bin/env python3
"""Real MySQL/PostgreSQL acceptance. Uses only dedicated test databases."""
import argparse, json, os, subprocess, time, threading, queue, tempfile
from pathlib import Path

class Client:
    def __init__(self, binary):
        self.p = subprocess.Popen([str(binary), '--protocol', '1'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.seq = 0
        self.send({'type':'handshake','request_id':'handshake','runtime_version':'1.4.4'})
        assert self.read()['type']=='handshake_result'
        assert self.read()['type']=='register'
    def send(self, value):
        value.update(extension_id='database',protocol_version='1.0')
        self.p.stdin.write(json.dumps(value)+'\n');self.p.stdin.flush()
    def read(self):
        line=self.p.stdout.readline()
        assert line, f'extension exited: {self.p.poll()}'
        return json.loads(line)
    def call(self, op, *args, scope='test', error=None):
        self.seq+=1
        self.send(dict(type='call', request_id=str(self.seq), callable='database.'+op, arguments=list(args),scope=scope))
        r=self.read()
        assert r['request_id']==str(self.seq), r
        if error:
            assert r['type']=='error' and r['code']==error, r
            return r
        assert r['type']=='call_result',r
        return r['value']
    def end_scope(self, scope):
        self.send(dict(type='scope_end',request_id='end',scope=scope));assert self.read()['type']=='scope_ended'
    def close(self):
        self.send(dict(type='shutdown',request_id='shutdown'));self.p.stdin.close()
        assert self.p.wait(timeout=10)==0

def exercise(c,cfg):
    my=cfg['driver']=='mysql';mark='?' if my else '$1'
    scalar=lambda sql, params=[]:c.call('query',cfg,sql,params)['rows'][0]
    c.call('ping',cfg)
    id_sql='SELECT CONNECTION_ID() AS id' if my else 'SELECT pg_backend_pid() AS id'
    assert scalar(id_sql)==scalar(id_sql),'connection was not reused'
    assert c.call('stats')['created']==1
    c.call('execute',cfg,'DROP TABLE IF EXISTS hhy_db_acceptance',[])
    c.call('execute',cfg,'CREATE TABLE hhy_db_acceptance (id INTEGER PRIMARY KEY, amount DECIMAL(30,10), label VARCHAR(200), data '+('BLOB' if my else 'BYTEA')+')',[])
    c.call('execute',cfg,'INSERT INTO hhy_db_acceptance(id,amount,label) VALUES (1,12345678901234567890.1234567890,'+mark+')',["HHY ' 中文 🐈"])
    assert scalar('SELECT label, amount FROM hhy_db_acceptance WHERE id=1')['amount']=='12345678901234567890.1234567890'
    c.call('execute',cfg,'INSERT INTO hhy_db_acceptance(id) VALUES (1)',[],error='DB_CONSTRAINT')
    assert scalar('SELECT COUNT(*) AS n FROM hhy_db_acceptance')['n']=='1'
    tx=c.call('begin',cfg)
    c.call('execute',tx,'INSERT INTO hhy_db_acceptance(id) VALUES (2)',[])
    c.call('savepoint',tx,'sp1')
    c.call('execute',tx,'INSERT INTO hhy_db_acceptance(id) VALUES (3)',[])
    c.call('rollback_to',tx,'sp1')
    assert c.call('query',tx,'SELECT COUNT(*) AS n FROM hhy_db_acceptance',[])['rows'][0]['n']=='2'
    c.call('commit',tx)
    c.call('query',tx,'SELECT 1',[],error='DB_RESOURCE')
    tx=c.call('begin',cfg,scope='owner')
    c.call('execute',tx,'INSERT INTO hhy_db_acceptance(id) VALUES (4)',[],scope='owner')
    c.call('query',tx,'SELECT 1',[],scope='intruder',error='DB_RESOURCE')
    c.end_scope('owner')
    assert scalar('SELECT COUNT(*) AS n FROM hhy_db_acceptance')['n']=='2'
    # A statement error can be recovered at a savepoint without losing the transaction.
    tx=c.call('begin',cfg)
    c.call('savepoint',tx,'recover')
    c.call('execute',tx,'INSERT INTO hhy_db_acceptance(id) VALUES (1)',[],error='DB_CONSTRAINT')
    c.call('rollback_to',tx,'recover')
    c.call('commit',tx)
    prepared=c.call('prepare',cfg,'SELECT '+mark+' AS x')
    for value in ['first','second']:
        assert c.call('query_prepared',prepared,[value])['rows']==[{'x':value}]
    c.call('close',prepared)
    c.call('execute_many',cfg,'INSERT INTO hhy_db_acceptance(id) VALUES ('+mark+')',[[5],[6]])
    c.call('transaction',cfg,[{'sql':'INSERT INTO hhy_db_acceptance(id) VALUES (7)','params':[]},{'sql':'INSERT INTO hhy_db_acceptance(id) VALUES (5)','params':[]}],error='DB_CONSTRAINT')
    assert scalar('SELECT COUNT(*) AS n FROM hhy_db_acceptance')['n']=='4'
    c.call('execute',cfg,'UPDATE hhy_db_acceptance SET data='+mark+' WHERE id=1',[{'type':'bytes','value':'00ff0100'}])
    assert scalar('SELECT data FROM hhy_db_acceptance WHERE id=1')['data']=={'type':'bytes','value':'00ff0100'}
    typed=c.call('query',cfg,'SELECT id,amount FROM hhy_db_acceptance WHERE id=1',[],{'typed':True})['rows'][0]
    assert typed['id']==1 and typed['amount']=={'type':'decimal','value':'12345678901234567890.1234567890'}
    c.call('query',cfg,'SELECT 1 AS x, 2 AS x',[],error='DB_DUPLICATE_COLUMN')
    assert c.call('query',cfg,'SELECT 1 AS x, 2 AS x',[],{'positional':True})['rows']==[['1','2']]
    cur=c.call('cursor',cfg,'SELECT id FROM hhy_db_acceptance ORDER BY id',[])
    all_rows=[]
    while True:
        batch=c.call('fetch',cur,2);all_rows+=batch['rows']
        if batch['done']:break
    assert [r['id'] for r in all_rows]==['1','2','5','6']
    c.call('close',cur)
    one=dict(cfg,max_open=1,max_idle=1,acquire_ms=30)
    tx=c.call('begin',one)
    c.call('ping',one,error='DB_POOL_TIMEOUT')
    c.call('rollback',tx)
    slow='SELECT SLEEP(5)' if my else 'SELECT pg_sleep(5)'
    started=time.monotonic()
    c.call('query',cfg,slow,[],{'timeout_ms':100},error='DB_TIMEOUT')
    assert time.monotonic()-started<3,'deadline failed'
    c.call('ping',cfg)
    if my:
        c.call('execute',cfg,'DROP PROCEDURE IF EXISTS hhy_db_proc',[])
        c.call('execute',cfg,'CREATE PROCEDURE hhy_db_proc() BEGIN SELECT 10 AS x; SELECT 20 AS x; END',[])
        cur=c.call('cursor',cfg,'CALL hhy_db_proc()',[])
        assert c.call('fetch',cur)['rows']==[{'x':'10'}]
        assert c.call('next_result',cur)
        assert c.call('fetch',cur)['rows']==[{'x':'20'}]
        assert c.call('next_result',cur)
        assert c.call('fetch',cur)['rows']==[]
        assert not c.call('next_result',cur)
        c.call('close',cur)
        c.call('execute',cfg,'DROP PROCEDURE hhy_db_proc',[])
    # Same process concurrent calls must correlate independently and stay below pool cap.
    for i in range(8):
        c.send(dict(type='call',request_id='parallel-'+str(i),callable='database.query',arguments=[cfg,'SELECT 42 AS value',[]],scope='parallel'))
    received=[c.read() for _ in range(8)]
    assert {r['request_id'] for r in received}=={'parallel-'+str(i) for i in range(8)}
    assert all(r['type']=='call_result' and r['value']['rows']==[{'value':'42'}] for r in received),received
    c.end_scope('parallel')
    c.call('execute',cfg,'DROP TABLE hhy_db_acceptance',[])
    return c.call('stats')

def main():
    p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=Path('extensions/database/bin/hhy-database'));p.add_argument('--config',type=Path,required=True);args=p.parse_args()
    configs=json.loads(args.config.read_text())
    reports=[]
    for cfg in configs:
        c=Client(args.binary.resolve())
        try:reports.append({'driver':cfg['driver'],'stats':exercise(c,cfg),'passed':True})
        finally:c.close()
    print(json.dumps({'acceptance':reports},indent=2))
if __name__=='__main__':main()
