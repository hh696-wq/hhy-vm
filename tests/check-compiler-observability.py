#!/usr/bin/env python3
"""Managed allocations, source attribution, dry-run and cancellation with IR."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else 'build/hhy').resolve())
env = {k: v for k, v in os.environ.items() if not k.startswith('HHY_COMPILER')}
with tempfile.TemporaryDirectory(prefix='hhy-compiler-observe-') as temp:
    temp = Path(temp)
    source = temp/'program.hhy'
    source.write_text('fn f(x) { let a = 10\nlet b = a\n1 + 2\nreturn [x, b + 3] }\nprint(f(4))\n')
    profiles = []
    outputs = []
    for mode in ('bc', 'ir'):
        report = temp/'profile.json'
        result = subprocess.run([binary, 'profile', '--engine', 'bytecode', '--heap', '--format', 'json',
            '--output', str(report), str(source)], env={**env, 'HHY_COMPILER': mode},
            capture_output=True, timeout=30)
        assert result.returncode == 0, result.stderr
        profiles.append(json.loads(report.read_text()))
        outputs.append((result.returncode, result.stdout, result.stderr))
    assert outputs[0] == outputs[1]
    for key in ('allocated_bytes', 'allocations', 'frame_pool', 'call_unwind'):
        assert profiles[0][key] == profiles[1][key], (key, profiles[0][key], profiles[1][key])
    # Compare call attribution, not dispatch counts or sampled/timed performance.
    def attribution(profile):
        return sorted((h['name'], h.get('path'), h.get('line'), h.get('column'), h.get('calls'))
                      for h in profile['hotspots'])
    assert attribution(profiles[0]) == attribution(profiles[1])

    destination = temp/'must-not-exist'
    source.write_text('print(20 + 22)\nwrite_text(path(args[0]), "unchanged")\n')
    outputs = []
    for mode in ('bc', 'ir'):
        result = subprocess.run([binary, 'run', '--engine', 'bytecode', '--dry-run', str(source), str(destination)],
            env={**env, 'HHY_COMPILER': mode}, capture_output=True, timeout=30)
        assert result.returncode == 0, result.stderr
        outputs.append((result.returncode, result.stdout, result.stderr))
        assert not destination.exists()
    assert outputs[0] == outputs[1]

    for mode in ('bc', 'ir'):
        for repetition in range(3):
            ready = temp/'ready'
            ready.unlink(missing_ok=True)
            source.write_text('write_text(path(args[0]), "ready")\nwhile true { 20 + 22 }\n')
            process = subprocess.Popen([binary, 'run', '--engine', 'bytecode', str(source), str(ready)],
                env={**env, 'HHY_COMPILER': mode}, stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
            try:
                deadline = time.monotonic() + 10
                while not ready.exists():
                    assert process.poll() is None, process.communicate()
                    assert time.monotonic() < deadline
                    time.sleep(.005)
                started = time.monotonic()
                process.send_signal(signal.SIGINT)
                stdout, stderr = process.communicate(timeout=5)
                assert process.returncode == 5 and b'cancel' in stderr.lower(), (stdout, stderr)
                assert time.monotonic() - started < 5
            finally:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.communicate(timeout=5)
print('compiler: exact managed allocation/call attribution, dry-run, six initialized SIGINT checks passed')
