#!/usr/bin/env python3
"""Measure native extension RSS for incremental results; never buffers all rows."""
import argparse,json,subprocess,sys,time
from pathlib import Path
from integration import Client

def rss(pid):
    if sys.platform.startswith('linux'):
        for line in Path(f'/proc/{pid}/status').read_text().splitlines():
            if line.startswith('VmRSS:'):return int(line.split()[1])*1024
    return int(subprocess.check_output(['ps','-o','rss=','-p',str(pid)],text=True).strip())*1024

def main():
    p=argparse.ArgumentParser();p.add_argument('--config',type=Path,required=True);p.add_argument('--rows',type=int,default=100000);a=p.parse_args()
    for cfg in json.loads(a.config.read_text()):
        cfg=dict(cfg,timeout_ms=300000)
        c=Client(Path('extensions/database/bin/hhy-database').resolve())
        try:
            results=[]
            for count in [max(1000,a.rows//10),a.rows]:
                if cfg['driver']=='mysql':
                    digits='(SELECT 0 n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9)'
                    sql='SELECT 42 AS id, REPEAT(\'x\',64) AS payload FROM '+', '.join(digits+' d'+str(i) for i in range(6))+' LIMIT '+str(count)
                else:sql=f"SELECT n AS id, REPEAT('x',64) AS payload FROM generate_series(1,{count}) n"
                before=rss(c.p.pid);peak=before;start=time.monotonic();cur=c.call('cursor',cfg,sql,[]);seen=0
                while True:
                    batch=c.call('fetch',cur,500);seen+=len(batch['rows'])
                    if seen%5000==0:peak=max(peak,rss(c.p.pid))
                    if batch['done']:break
                c.call('close',cur);assert seen==count,(seen,count)
                results.append(dict(rows=seen,seconds=round(time.monotonic()-start,3),rss_before=before,rss_peak=peak))
            # A tenfold result increase may grow caches modestly, never by the full result size.
            assert results[1]['rss_peak']-results[0]['rss_peak']<32*1024*1024,results
            assert c.call('stats')['pinned']==0
            print(json.dumps(dict(driver=cfg['driver'],streaming=results)))
        finally:c.close()
if __name__=='__main__':main()
