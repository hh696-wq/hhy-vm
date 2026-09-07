#!/usr/bin/env python3
"""Installation must evaluate numeric runtime ranges, including upper bounds."""
import pathlib
import shutil
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())
version = pathlib.Path('VERSION').read_text().strip()
major, minor, patch = map(int, version.split('.'))
cases = {
    '>=1.1,<2.0': True,
    f'>={version},<{major + 1}.0': True,
    f'={version}': True,
    f'>{version}': False,
    f'<{version}': False,
    f'>={major}.{minor}.{patch + 1}': False,
    '>=1.1,<1.2': False,
    '>=1.1garbage': False,
    '>=1.1,': False,
    '>=1.1.0.0': False,
    '>=1.1,wat': False,
}
with tempfile.TemporaryDirectory(prefix='hhy-package-versions-') as temp:
    package = pathlib.Path(temp) / 'sample'
    shutil.copytree('extensions/sample', package)
    manifest = package / 'hhy.toml'
    original = manifest.read_text()
    import re
    for requirement, accepted in cases.items():
        manifest.write_text(re.sub(r'requires_hhy = "[^"]+"',
                                  f'requires_hhy = "{requirement}"', original))
        result = subprocess.run([binary, 'install', '--dry-run', str(package)],
                                capture_output=True, text=True)
        assert (result.returncode == 0) == accepted, (requirement, result.stderr)
print('package runtime version range tests passed')
