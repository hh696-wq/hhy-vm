#!/usr/bin/env python3
"""Bundle the complete non-system dylib closure, then sign relocated copies."""
import hashlib,re,shutil,subprocess,sys
from pathlib import Path
stage=Path(sys.argv[1]).resolve();libdir=stage/'lib';libdir.mkdir(exist_ok=True)
executables=[stage/'bin/hhy',*stage.glob('extensions/*/bin/*')]
sources={};pending=[]
def output(*args):return subprocess.check_output(args,text=True)
def dependencies(path):return [line.strip().split(' (compatibility',1)[0] for line in output('otool','-L',str(path)).splitlines()[1:]]
def resolve(source,dep):
    if dep.startswith(('/usr/lib/','/System/Library/')):return None
    if dep.startswith('@loader_path/') or dep.startswith('@executable_path/'):
        result=source.parent/dep.split('/',1)[1]
    elif dep.startswith('@rpath/'):
        candidates=[source.parent/dep.split('/',1)[1]]
        lines=output('otool','-l',str(source)).splitlines()
        for i,line in enumerate(lines):
            if line.strip()=='cmd LC_RPATH':
                for following in lines[i+1:i+5]:
                    m=re.match(r'\s*path (.+) \(offset',following)
                    if m:
                        root=m[1].replace('@loader_path',str(source.parent)).replace('@executable_path',str(source.parent))
                        candidates.append(Path(root)/dep.split('/',1)[1])
        result=next((p for p in candidates if p.exists()),candidates[0])
    else:result=Path(dep)
    if not result.is_file():raise RuntimeError(f'unresolved dependency {dep} from {source.name}')
    return result.resolve()
def register(source):
    name=source.name
    if name in sources:
        if hashlib.sha256(source.read_bytes()).digest()!=hashlib.sha256(sources[name].read_bytes()).digest():
            raise RuntimeError(f'conflicting dylibs named {name}')
        return libdir/name
    sources[name]=source;dest=libdir/name;shutil.copyfile(source,dest);dest.chmod(0o755);pending.append((source,dest));return dest
def relocate(source,dest,executable):
    for dep in dependencies(source):
        resolved=resolve(source,dep)
        if resolved is None:continue
        target=register(resolved)
        replacement=('@executable_path/../lib/' if executable else '@loader_path/')+target.name
        subprocess.run(['install_name_tool','-change',dep,replacement,str(dest)],check=True,capture_output=True)
    if not executable:subprocess.run(['install_name_tool','-id','@loader_path/'+dest.name,str(dest)],check=True,capture_output=True)
for executable in executables:relocate(executable,executable,True)
while pending:
    source,dest=pending.pop();relocate(source,dest,False)
for path in [*libdir.iterdir(),*executables]:
    if path.is_file():
        subprocess.run(['codesign','--force','--sign','-',str(path)],check=True,capture_output=True)
        assert all(not d.startswith(('/opt/homebrew/','/usr/local/')) for d in dependencies(path)),path
for extension in stage.glob('extensions/*'):
    if (extension/'bin').is_dir():shutil.copytree(libdir,extension/'lib',dirs_exist_ok=True)
print(f'Bundled and verified {len(sources)} transitive dylibs')
