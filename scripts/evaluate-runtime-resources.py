#!/usr/bin/env python3
"""Local GC and cancellation evidence; observational measurements, not invented SLAs."""
import argparse,hashlib,json,os,platform,signal,statistics,subprocess,tempfile,time
from pathlib import Path

def percentile(values, fraction):
    values=sorted(values)
    return values[min(len(values)-1,int((len(values)-1)*fraction))] if values else None

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',default='build/hhy')
    parser.add_argument('--probe',default='build/hhy-resource-probe')
    parser.add_argument('--iterations',type=int,default=15)
    parser.add_argument('--output',default='build/benchmarks/runtime-resources.json')
    args=parser.parse_args()
    if args.iterations<3: parser.error('at least 3 samples required')
    binary=str(Path(args.binary).resolve());probe=str(Path(args.probe).resolve())
    env={**os.environ,'HHY_GC_STRESS':'0','HHY_PROBE_LIVE_CONTEXT':'0','HHY_MAP_INLINE_CACHE':'0','HHY_CALL_FRAME_POOL':'legacy','HHY_CALL_FRAME_UNWIND':'0','HHY_BYTECODE_CALL_PLANS':'0','HHY_BYTECODE_EXCEPTION_TABLES':'0'}
    gc=[];cancellation=[];parallel=[]
    with tempfile.TemporaryDirectory(prefix='hhy-resources-') as temp:
        temp=Path(temp);report=temp/'gc.json'
        for engine in ('ast','bytecode'):
            for source in ('benchmarks/json-flow.hhy','benchmarks/core-flow.hhy','benchmarks/gc-retention.hhy'):
                samples=[]
                oracle=subprocess.run([binary,'run','--engine',engine,source],env=env,capture_output=True,check=True,timeout=60)
                for i in range(args.iterations+2):
                    run=subprocess.run([probe,str(report),engine,source],env=env,capture_output=True,timeout=60)
                    if (run.returncode,run.stdout,run.stderr)!=(oracle.returncode,oracle.stdout,oracle.stderr):raise RuntimeError((engine,source,run.stderr))
                    if i>=2:samples.append(json.loads(report.read_text()))
                pauses=[n for s in samples for n in s['pause_samples_ns']]
                gc.append({'engine':engine,'source':source,'samples':samples,'allocation_bytes_per_second_median':statistics.median(s['allocated_bytes']/(s['elapsed_ns']/1e9) for s in samples),'pause_p50_ns':percentile(pauses,.5),'pause_p95_ns':percentile(pauses,.95),'pause_p99_ns':percentile(pauses,.99),'observed_max_pause_ns':max(s['max_pause_ns'] for s in samples),'gc_pause_share_median':statistics.median(s['pause_ns']/s['elapsed_ns'] for s in samples)})
            run=subprocess.run([probe,str(report),engine,'benchmarks/gc-retention.hhy'],env={**env,'HHY_PROBE_LIVE_CONTEXT':'1'},capture_output=True,check=True,timeout=60)
            gc.append({'engine':engine,'source':'benchmarks/gc-retention.hhy','context_retention_probe':json.loads(report.read_text())})
            for fixture in ('cancel.hhy','parallel-cancel.hhy'):
                samples=[]
                for i in range(args.iterations):
                    ready=temp/'ready'
                    ready.unlink(missing_ok=True)
                    source=temp/fixture
                    source.write_text('write_text(path(args[0]), "ready")\n'+(Path('tests/valid')/fixture).read_text())
                    process=subprocess.Popen([binary,'run','--engine',engine,str(source),str(ready)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,start_new_session=True)
                    try:
                        deadline=time.monotonic()+10
                        while not ready.exists():
                            if process.poll() is not None:raise RuntimeError(process.communicate())
                            if time.monotonic()>deadline:raise RuntimeError('ready timeout')
                            time.sleep(.005)
                        time.sleep(.1)
                        started=time.perf_counter_ns();process.send_signal(signal.SIGINT)
                        stdout,stderr=process.communicate(timeout=5)
                        elapsed=time.perf_counter_ns()-started
                        if process.returncode!=5:raise RuntimeError((process.returncode,stdout,stderr))
                        # Before emergency cleanup, ensure Runtime reaped its entire process group.
                        listing=subprocess.check_output(['ps','-axo','pid=,pgid='],text=True)
                        survivors=[line for line in listing.splitlines() if len(line.split())==2 and line.split()[1]==str(process.pid)]
                        if survivors:raise RuntimeError(('unreaped children',survivors))
                        samples.append(elapsed)
                    finally:
                        try:os.killpg(process.pid,signal.SIGKILL)
                        except ProcessLookupError:pass
                        process.communicate(timeout=5)
                cancellation.append({'engine':engine,'fixture':fixture,'samples_ns':samples,'p95_ns':percentile(samples,.95),'observed_max_ns':max(samples),'orphan_groups':0})
            for workers in (1,2,4):
                source=temp/'bounded.hhy'
                source.write_text('[3, 1, 2, 4] |> stream |> parallel('+str(workers)+') { n ->\nif n == 3 { sleep(80ms) } else { sleep(10ms) }\nreturn n * 2\n} |> collect |> print\n')
                samples=[]
                for i in range(args.iterations):
                    started=time.perf_counter_ns();run=subprocess.run([binary,'run','--engine',engine,str(source)],env=env,capture_output=True,timeout=10)
                    elapsed=time.perf_counter_ns()-started
                    if run.returncode or run.stdout!=b'[6, 2, 4, 8]\n':raise RuntimeError(run.stderr)
                    samples.append(elapsed)
                parallel.append({'engine':engine,'workers':workers,'ordered_output':True,'samples_ns':samples,'median_ns':statistics.median(samples)})
    result={'schema_version':1,'platform':platform.platform(),'machine':platform.machine(),'iterations':args.iterations,'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'production_binary_sha256':hashlib.sha256(Path(binary).read_bytes()).hexdigest(),'probe_sha256':hashlib.sha256(Path(probe).read_bytes()).hexdigest(),'gc':gc,'cancellation':cancellation,'parallel':parallel,'decision':{'collector':'retain_existing_conservative_collector','scheduler':'retain_bounded_ordered_process_parallelism','promotion_bytes':None,'write_barrier_cost':None,'production_pause_budget':None,'production_concurrency_sla':None,'cross_platform_admitted':False,'scope':'local_observation_and_conditional_decision_not_production_SLA_proof'}}
    output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,indent=2)+'\n')
    print(f'GC, cancellation and bounded ordering evidence saved to {output}')
if __name__=='__main__': main()
