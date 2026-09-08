#!/usr/bin/env python3
"""The profiler must not invent pairs across execution domains."""
import json
import subprocess
import sys
report = json.loads(subprocess.check_output([sys.argv[1]], text=True))['dispatch_profile']
counts = {tuple(x['opcodes']): x['count'] for x in report['sequences']}
assert report['total'] == 5
assert counts == {('PROGRAM',): 1, ('CALL',): 1, ('IDENTIFIER',): 1, ('LITERAL',): 1,
                  ('RETURN',): 1, ('PROGRAM', 'CALL'): 1, ('IDENTIFIER', 'LITERAL'): 1}, counts
print('dispatch boundary and invalid-input tests passed')
