"""Shared native NSIS compiler invocation and safe owned-file uninstall manifest."""
from pathlib import Path
import os
import subprocess

def compile_installer(root, payload, art, icon, output, makensis='makensis', nsis_dir=None):
    remove=payload.parent/'remove-files.nsh'
    files=sorted(p for p in payload.rglob('*') if p.is_file())
    lines=['; Generated: remove only files owned by this installer.']
    for file in files:
        relative=str(file.relative_to(payload)).replace('/', '\\').replace('$','$$')
        lines.append(f'Delete "$INSTDIR\\{relative}"')
    for folder in sorted((p for p in payload.rglob('*') if p.is_dir()),key=lambda p:len(p.parts),reverse=True):
        relative=str(folder.relative_to(payload)).replace('/','\\').replace('$','$$')
        lines.append(f'RMDir "$INSTDIR\\{relative}"')
    remove.write_text('\n'.join(lines)+'\n')
    definitions={'OUTPUT':output.resolve(),'PAYLOAD':payload.resolve(),'ART':art.resolve(),'ICON':icon.resolve(),'REMOVE_FILES':remove.resolve()}
    env=dict(os.environ)
    if nsis_dir:env['NSISDIR']=str(Path(nsis_dir).resolve())
    prefix='/D' if os.name=='nt' else '-D'
    subprocess.run([str(makensis),*[prefix+k+'='+str(v) for k,v in definitions.items()],str(root/'tools/nsis/installer.nsi')],check=True,env=env)
    print(output)
