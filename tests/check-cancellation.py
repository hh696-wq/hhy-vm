#!/usr/bin/env python3
"""Signal only after runtime initialization, and always reap fixture processes."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

binary = str(Path(sys.argv[1]).resolve())
for fixture in ['parallel-cancel.hhy', 'cancel.hhy']:
    with tempfile.TemporaryDirectory(prefix='hhy-cancellation-') as directory:
        directory = Path(directory)
        ready = directory/'ready'
        script = directory/fixture
        script.write_text('write_text(path(args[0]), "ready")\n' +
                          (Path('tests/valid')/fixture).read_text())
        process = subprocess.Popen([binary, 'run', str(script), str(ready)],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, start_new_session=True)
        try:
            deadline = time.monotonic()+10
            while not ready.exists():
                assert process.poll() is None, process.communicate()
                assert time.monotonic() < deadline, 'runtime initialization timed out'
                time.sleep(.01)
            # Initialization and signal-handler setup are complete; allow the
            # fixture to enter its waiting/parallel operation before interrupting.
            time.sleep(.1)
            process.send_signal(signal.SIGINT)
            stdout, stderr = process.communicate(timeout=5)
            assert process.returncode == 5, (fixture, process.returncode, stdout, stderr)
        finally:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.communicate(timeout=5)
print('Runtime and parallel Ctrl+C cancellation passed')
