#!/usr/bin/env python3
"""Extract independently installable DB packages from verified HHY archives."""
import hashlib,json,re,sys,tarfile,tomllib
from pathlib import Path,PurePosixPath
root=Path(__file__).resolve().parents[1]
version=tomllib.loads((root/'extensions/database/hhy.toml').read_text())['package']['version']
if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?',version):raise SystemExit('invalid database version')
source=Path(sys.argv[1]);dest=Path(sys.argv[2]);dest.mkdir(parents=True,exist_ok=True)
written=[]
for archive in sorted(source.glob('hhy-*.tar.gz')):
    if 'windows' in archive.name:continue
    match=re.search(r'-(darwin-arm64|linux-arm64|linux-x86_64)\.tar\.gz$',archive.name)
    if not match:continue
    platform=match[1];prefix=f'hhy-database-{version}-{platform}'
    target=dest/(prefix+'.tar.gz');found=set()
    with tarfile.open(archive,'r:gz') as incoming,tarfile.open(target,'w:gz') as outgoing:
        for member in incoming.getmembers():
            parts=PurePosixPath(member.name).parts
            if len(parts)<4 or parts[1:3]!=('extensions','database'):continue
            relative=parts[3:]
            if '..' in relative or member.issym() or member.islnk():raise SystemExit('unexpected database archive member')
            if not(member.isfile() or member.isdir()):raise SystemExit('unexpected archive member type')
            member.name='/'.join((prefix,*relative));member.uid=member.gid=0;member.uname=member.gname=''
            outgoing.addfile(member,incoming.extractfile(member) if member.isfile() else None)
            found.add('/'.join(relative))
    assert {'hhy.toml','bin/hhy-database','README.md','LICENSE','NOTICE'}<=found,found
    checksum=hashlib.sha256(target.read_bytes()).hexdigest()
    target.with_name(target.name+'.sha256').write_text(f'{checksum}  {target.name}\n')
    written.append(target.name)
assert len(written)==3,f'expected all three supported database platforms, got {written}'
(dest/'SHA256SUMS').write_text(''.join((dest/(name+'.sha256')).read_text() for name in written))
print(json.dumps({'version':version,'archives':written}))
