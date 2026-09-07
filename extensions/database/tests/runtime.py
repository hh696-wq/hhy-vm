#!/usr/bin/env python3
"""Test HHY AST/bytecode resource scopes and prefork database isolation."""
import argparse, concurrent.futures, json, os, signal, socket, subprocess, tempfile, time, urllib.request, urllib.error
from pathlib import Path

SCRIPT='''import database
let cfg = read_text(path(args[0])) |> parse_json
let timed = {driver: cfg.driver, host: cfg.host, port: cfg.port, user: cfg.user, password: cfg.password,
    database: cfg.database, tls: cfg.tls, allow: cfg.allow, timeout_ms: 2s, acquire_ms: 500ms}
database.ping(timed)
let bytes = read_bytes(path(args[1]))
let binary = database.query(cfg, args[3], [bytes], {typed: true})
write_bytes(path(args[2]), binary.rows[0].payload)
database.execute(cfg, "DROP TABLE IF EXISTS hhy_runtime_test", [])
database.execute(cfg, "CREATE TABLE hhy_runtime_test (id INT PRIMARY KEY)", [])
let inserted = cfg |> database.with_transaction { tx ->
    database.execute(tx, "INSERT INTO hhy_runtime_test VALUES (1)", [])
    database.query(tx, "SELECT COUNT(*) AS n FROM hhy_runtime_test", [])
}
let rolled = attempt {
    cfg |> database.with_transaction { tx ->
        database.execute(tx, "INSERT INTO hhy_runtime_test VALUES (2)", [])
        database.execute(tx, "INSERT INTO hhy_runtime_test VALUES (1)", [])
    }
}
let rows = database.stream(cfg, "SELECT id FROM hhy_runtime_test ORDER BY id", []) |> collect
let taken = database.stream(cfg, "SELECT id FROM hhy_runtime_test ORDER BY id", []) |> take(1) |> collect
let report = { committed: inserted.rows[0].n, failed: rolled.ok, rows: rows, taken: taken, stats: database.stats() }
report |> encode_json |> print
database.execute(cfg, "DROP TABLE hhy_runtime_test", [])
'''
WEB='''import web
import database
let cfg = read_text(path(args[0])) |> parse_json
let identity_sql = args[2]
database.execute(cfg, "DROP TABLE IF EXISTS hhy_scope_test", [])
database.execute(cfg, "CREATE TABLE hhy_scope_test (id INT PRIMARY KEY)", [])
database.query(cfg, identity_sql, []) |> encode_json |> print
fn identity(request) {
    web.json({ request: request.id, database: database.query(cfg, identity_sql, []) })
}
fn leak(request) {
    let tx = database.begin(cfg)
    database.execute(tx, "INSERT INTO hhy_scope_test VALUES (1)", [])
    web.json({ ok: true })
}
fn count_rows(request) {
    web.json(database.query(cfg, "SELECT COUNT(*) AS n FROM hhy_scope_test", []))
}
web.app() |> web.get("/identity", identity) |> web.get("/leak", leak) |> web.get("/count", count_rows)
    |> web.listen({host: "127.0.0.1", port: to_int(args[1]), workers: 2})
'''
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--hhy',type=Path,default=Path('build/hhy'));ap.add_argument('--config',type=Path,required=True);a=ap.parse_args()
    hhy=str(a.hhy.resolve());cfgs=json.loads(a.config.read_text())
    with tempfile.TemporaryDirectory(prefix='hhy-db-runtime-') as tmp:
        tmp=Path(tmp);env=dict(os.environ,HHY_EXTENSION_HOME=str(tmp/'extensions'),HHY_WEB_ACCESS_LOG='0')
        subprocess.run([hhy,'install','--yes','extensions/database'],env=env,check=True,stdout=subprocess.DEVNULL)
        script=tmp/'runtime.hhy';script.write_text(SCRIPT)
        web=tmp/'web.hhy';web.write_text(WEB)
        for cfg in cfgs:
            cp=tmp/'config.json';cp.write_text(json.dumps(cfg));cp.chmod(0o600)
            payload=tmp/'payload.bin';payload.write_bytes(bytes([0,255,128,39,92,10]))
            roundtrip=tmp/'roundtrip.bin'
            binary_sql='SELECT CAST(? AS BINARY) AS payload' if cfg['driver']=='mysql' else 'SELECT $1::bytea AS payload'
            for engine in ['ast','bytecode']:
                result=subprocess.run([hhy,'run','--engine',engine,str(script),str(cp),str(payload),str(roundtrip),binary_sql],env=env,check=False,text=True,capture_output=True,timeout=30)
                assert result.returncode==0,result.stderr
                assert roundtrip.read_bytes()==payload.read_bytes(),'native BytesBuffer round trip failed'
                r=json.loads(result.stdout)
                assert r['committed']=='1' and r['failed'] is False and r['rows']==[{'id':'1'}] and r['taken']==[{'id':'1'}],r
                assert r['stats']['pinned']==0,r
            with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
            identity='SELECT CONNECTION_ID() AS id' if cfg['driver']=='mysql' else 'SELECT pg_backend_pid() AS id'
            p=subprocess.Popen([hhy,'serve',str(web),'--',str(cp),str(port),identity],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            def get(path):
                with urllib.request.urlopen(f'http://127.0.0.1:{port}{path}',timeout=10) as response:return json.load(response)
            try:
                deadline=time.monotonic()+10
                while True:
                    try:get('/identity');break
                    except Exception:
                        if p.poll() is not None or time.monotonic()>deadline:raise RuntimeError('Web database fixture did not start')
                        time.sleep(.05)
                with concurrent.futures.ThreadPoolExecutor(8) as pool:responses=list(pool.map(lambda _:get('/identity'),range(40)))
                by_worker={}
                for r in responses:by_worker.setdefault(r['request'].split('-')[0],set()).add(r['database']['rows'][0]['id'])
                assert len(by_worker)==2 and all(len(ids)==1 for ids in by_worker.values()),by_worker
                ids=[next(iter(v)) for v in by_worker.values()];assert ids[0]!=ids[1],by_worker
                # Kill only an extension owned by one of this fixture's known workers.
                victim_worker=next(iter(by_worker))
                processes=subprocess.check_output(['ps','-axo','pid=,ppid=,comm='],text=True)
                children=[int(parts[0]) for line in processes.splitlines() if len(parts:=line.split(None,2))==3 and parts[1]==victim_worker and 'hhy-database' in parts[2]]
                assert len(children)==1,children
                os.kill(children[0],signal.SIGKILL)
                restored={}
                for _ in range(50):
                    try:r=get('/identity')
                    except urllib.error.HTTPError as error:
                        assert error.code==500;continue
                    restored.setdefault(r['request'].split('-')[0],set()).add(r['database']['rows'][0]['id'])
                assert set(restored)==set(by_worker),'extension crash killed/replaced the Web worker'
                assert next(iter(restored[victim_worker])) not in by_worker[victim_worker],restored
                get('/leak')
                assert get('/count')['rows'][0]['n']=='0','request-scope cleanup did not roll back'
            finally:
                p.terminate()
                try:out,err=p.communicate(timeout=15)
                except subprocess.TimeoutExpired:p.kill();out,err=p.communicate();raise
            print(f'{cfg["driver"]}: AST, Bytecode, Stream, transaction rollback, two-Worker isolation and scope cleanup passed')
if __name__=='__main__':main()
