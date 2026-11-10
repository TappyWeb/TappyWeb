#!/usr/bin/env python3
"""Turn the Windows portable CLI payload into a branded NSIS Setup.exe."""
from pathlib import Path
import argparse
import shutil
import tempfile
import zipfile
from nsis_builder import compile_installer

ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--makensis',default='makensis')
    parser.add_argument('--nsis-dir',type=Path)
    parser.add_argument('--path-helper',type=Path,required=True)
    args=parser.parse_args()
    if args.path_helper.read_bytes()[:2]!=b'MZ':raise SystemExit('A Windows tappy-env.exe is required')
    with tempfile.TemporaryDirectory(prefix='tappy-nsis-') as temp:
        payload=Path(temp)/'payload';payload.mkdir()
        with zipfile.ZipFile(ROOT/'releases/Tappyweb-0.2.0-windows-x64.zip') as archive:archive.extractall(payload)
        shutil.copy2(args.path_helper,payload/'EnvPath.exe')
        shutil.copy2(ROOT/'assets/installer/AppIcon.ico',payload/'AppIcon.ico')
        for name in ('install.ps1','START-HERE.md'):(payload/name).unlink(missing_ok=True)
        compile_installer(ROOT,payload,ROOT/'assets/installer/sidebar.bmp',ROOT/'assets/installer/AppIcon.ico',ROOT/'releases/Tappyweb-0.2.0-Windows-x64-Setup.exe',args.makensis,args.nsis_dir)
if __name__=='__main__':main()
