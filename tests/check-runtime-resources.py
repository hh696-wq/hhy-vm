#!/usr/bin/env python3
"""Diagnostic hooks preserve observable behavior and release their global slot."""
import json,os,subprocess,sys,tempfile
from pathlib import Path
probe=sys.argv[1] if len(sys.argv)>1 else 'build/hhy-resource-probe'
count=0
with tempfile.TemporaryDirectory(prefix='hhy-gc-probe-test-') as directory:
    report=Path(directory)/'report.json'
    for engine in ('ast','bytecode'):
        for source in ('benchmarks/gc-retention.hhy','tests/valid/closure-unwind-matrix.hhy','tests/invalid-runtime/parallel-error.hhy'):
            oracle=subprocess.run(['build/hhy','run','--engine',engine,source],capture_output=True,timeout=60)
            measured=subprocess.run([probe,str(report),engine,source],capture_output=True,timeout=60)
            assert (measured.returncode,measured.stdout,measured.stderr)==(oracle.returncode,oracle.stdout,oracle.stderr),(engine,source,measured.stderr)
            d=json.loads(report.read_text())
            assert d['callback_restored'] and d['collections']==d['gc_cycle_delta']
            assert d['pause_count']==len(d['pause_samples_ns'])+d['samples_dropped']
            assert d['max_pause_ns']<=d['pause_ns']<=d['collection_ns']<=d['elapsed_ns']
            assert d['promotion_bytes'] is None and d['write_barrier_ns'] is None
            count+=1
        measured=subprocess.run([probe,str(report),engine,'benchmarks/gc-retention.hhy'],env={**os.environ,'HHY_PROBE_LIVE_CONTEXT':'1'},capture_output=True,timeout=60)
        assert measured.returncode==0,measured.stderr
        d=json.loads(report.read_text());assert d['live_context_retained'] and d['post_run_retained_estimate_bytes']>20000*8,d
        count+=1
print(f'resource probe: {count} semantic, callback and retained-context checks passed')
