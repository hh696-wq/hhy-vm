#!/usr/bin/env python3
"""Prototype IR costs; never labels a diagnostic speedup as Runtime benefit."""
import argparse,hashlib,json,platform,statistics,subprocess,tempfile,time
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe',default='build/hhy-ir-probe')
    parser.add_argument('--output',default='build/benchmarks/compiler-ir.json')
    args=parser.parse_args();rows=[]
    expressions={'small':'(20 + 1) * 2','tree':' + '.join(str(i) for i in range(1,65)),'overflow':'9223372036854775807 + 1','first_error':'(1 % 0) + (9223372036854775807 + 1)','capacity_refusal':' + '.join('1' for _ in range(140))}
    with tempfile.TemporaryDirectory(prefix='hhy-ir-cost-') as directory:
        source=Path(directory)/'probe.hhy'
        for name,expression in expressions.items():
            source.write_text('print('+expression+')\n');samples={'--no-fold':[],'--fold':[]};reports={}
            for i in range(17):
                for flag in (('--no-fold','--fold') if i%2==0 else ('--fold','--no-fold')):
                    start=time.perf_counter_ns();raw=subprocess.check_output([args.probe,flag,str(source)],timeout=10);wall=time.perf_counter_ns()-start
                    report=json.loads(raw);reports[flag]=report
                    if i>=2:samples[flag].append({'wall_ns':wall,'lower_verify_ns':report['lower_verify_ns'],'fold_verify_ns':report['fold_verify_ns'],'serialized_bytes':len(raw)})
            rows.append({'case':name,'source_text':source.read_text(),'samples':samples,'reports':reports,'median_ns':{flag:{key:statistics.median(x[key] for x in values) for key in ('wall_ns','lower_verify_ns','fold_verify_ns')} for flag,values in samples.items()}})
        source.write_text('print((20 + 1) * 2)\n')
        text_dump=subprocess.check_output([args.probe,'--text',str(source)],text=True)
    result={'schema_version':1,'platform':platform.platform(),'iterations':15,'probe_sha256':hashlib.sha256(Path(args.probe).read_bytes()).hexdigest(),'production_binary_sha256':hashlib.sha256(Path('build/hhy').read_bytes()).hexdigest(),'rows':rows,'text_dump':text_dump,'decision':{'runtime_integrated':False,'runtime_speedup_measured':False,'full_cfg_verified':False,'v1.7.0_complete':False,'default_admitted':False,'scope':'bounded_single_block_i64_research_only'}}
    output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,indent=2)+'\n')
    print(f'IR prototype cost and refusal evidence saved to {output}')
if __name__=='__main__':main()
