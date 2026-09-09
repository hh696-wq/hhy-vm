#!/usr/bin/env python3
"""Validate the tracked Core surface, without relying on ignored local files."""
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {'.github', 'src', 'include', 'compiler', 'tests', 'benchmarks',
           'scripts', 'extensions', 'examples', 'docs', 'Formula'}

def main():
    files = set(subprocess.check_output(['git', 'ls-files'], cwd=ROOT, text=True).splitlines())
    errors = []
    for name in sorted(files):
        if '/' in name and name.split('/')[0] not in ALLOWED:
            errors.append(f'unsupported root directory: {name}')
        if any(part in {'node_modules', '__pycache__', '.venv'} for part in Path(name).parts):
            errors.append(f'generated dependency/cache: {name}')
        if not name.endswith('.md'):
            continue
        path = ROOT / name
        if not path.exists():
            errors.append(f'tracked file missing: {name}')
            continue
        for dest in re.findall(r'\]\(([^\s)]+)(?:\s+"[^"]*")?\)', path.read_text()):
            if dest.startswith('#') or re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:', dest):
                continue
            target = (path.parent / unquote(dest.split('#')[0])).resolve()
            try:
                relative = target.relative_to(ROOT).as_posix()
            except ValueError:
                errors.append(f'link outside repository: {name}: {dest}')
                continue
            if relative != '.' and relative not in files and not any(f.startswith(relative.rstrip('/') + '/') for f in files):
                errors.append(f'link target not tracked: {name}: {dest}')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f'Repository scope and Markdown links passed ({len(files)} tracked files).')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
