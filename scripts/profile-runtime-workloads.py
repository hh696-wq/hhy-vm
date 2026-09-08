#!/usr/bin/env python3
"""Run official self-tests while measuring each supported parent Runtime invocation."""
import argparse,hashlib,json,os,platform,subprocess,sys,tempfile
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',default='build/benchmarks/runtime-workload-resources')
    args=parser.parse_args()
    output=Path(args.output).resolve();output.mkdir(parents=True,exist_ok=True)
    binary=str(Path('build/hhy').resolve());probe=str(Path('build/hhy-resource-probe').resolve())
    results=[]
    cases=('asset-governance','dataflow-etl','flowguard','hong-kong-film-companies','multi-api-data-collector','sitegraph-auditor')
    with tempfile.TemporaryDirectory(prefix='hhy-resource-wrapper-') as temp:
        wrapper=Path(temp)/'hhy'
        wrapper.write_text('#!'+sys.executable+'\n'+'''import hashlib,json,os,sys
from pathlib import Path
binary='''+repr(binary)+'''
probe='''+repr(probe)+'''
output=Path(os.environ['HHY_RESOURCE_REPORT_DIRECTORY'])
args=sys.argv[1:]
if not args or args[0]!='run': os.execv(binary,[binary,*args])
rest=args[1:];engine=os.environ.get('HHY_ENGINE','bytecode')
if len(rest)>1 and rest[0]=='--engine': engine=rest[1];rest=rest[2:]
meta={'engine':engine,'argv':rest}
if not rest or rest[0].startswith('--'):
    meta['delegated']=True
    (output/(str(os.getpid())+'.meta.json')).write_text(json.dumps(meta))
    os.execv(binary,[binary,*args])
meta['delegated']=False
meta['source_sha256']=hashlib.sha256(Path(rest[0]).read_bytes()).hexdigest()
(output/(str(os.getpid())+'.meta.json')).write_text(json.dumps(meta))
os.execv(probe,[probe,str(output/(str(os.getpid())+'.gc.json')),engine,*rest])
''')
        wrapper.chmod(0o755)
        for engine in ('ast','bytecode'):
            for case in cases:
                directory=output/(engine+'-'+case);directory.mkdir(exist_ok=True)
                if any(directory.glob('*.meta.json')):raise RuntimeError('choose a fresh output directory to avoid mixing runs')
                run=subprocess.run(['sh',f'practical-projects/{case}/self-test.sh'],env={**os.environ,'HHY_BIN':str(wrapper),'HHY_ENGINE':engine,'HHY_RESOURCE_REPORT_DIRECTORY':str(directory),'HHY_PROBE_LIVE_CONTEXT':'0','HHY_GC_STRESS':'0'},capture_output=True,text=True,timeout=180)
                (directory/'stdout.log').write_text(run.stdout);(directory/'stderr.log').write_text(run.stderr)
                measurements=[]
                for meta in sorted(directory.glob('*.meta.json')):
                    entry=json.loads(meta.read_text())
                    if not entry['delegated']:
                        entry['measurement']=json.loads(meta.with_name(meta.name.replace('.meta.json','.gc.json')).read_text())
                    measurements.append(entry)
                results.append({'engine':engine,'project':case,'exit_code':run.returncode,'invocations':measurements})
                print(engine,case,'passed' if run.returncode==0 else 'FAILED',flush=True)
                if run.returncode:raise RuntimeError(run.stderr)
    (output/'summary.json').write_text(json.dumps({'schema_version':1,'platform':platform.platform(),'passed':True,'results':results,'browser_project':'not_run_browser_dependency_not_enabled','scope':'fixture_self_tests_parent_GC_only_no_production_SLA'},indent=2)+'\n')
if __name__=='__main__':main()
