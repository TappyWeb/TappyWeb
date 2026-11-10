#!/usr/bin/env python3
"""Package a prebuilt Windows CLI from any host, without executing it."""
from pathlib import Path
import argparse
import shutil
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def require_windows_x64(path):
    import struct
    data = Path(path).read_bytes()
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError(f'Not a Windows executable: {path}')
    offset = struct.unpack_from('<I', data, 60)[0]
    if data[offset:offset+4] != b'PE\0\0' or struct.unpack_from('<H', data, offset+4)[0] != 0x8664:
        raise ValueError(f'Expected a Windows x64 executable: {path}')

def build_installer(path_helper, makensis='makensis', nsis_dir=None):
    from nsis_builder import compile_installer
    require_windows_x64(path_helper)
    with tempfile.TemporaryDirectory(prefix='tappy-nsis-') as temp:
        payload=Path(temp)/'payload';payload.mkdir()
        with zipfile.ZipFile(ROOT/'releases/Tappyweb-0.3.1-windows-x64.zip') as archive:archive.extractall(payload)
        shutil.copy2(path_helper,payload/'EnvPath.exe')
        shutil.copy2(ROOT/'assets/installer/AppIcon.ico',payload/'AppIcon.ico')
        for name in ('install.ps1','START-HERE.md'):(payload/name).unlink(missing_ok=True)
        compile_installer(ROOT,payload,ROOT/'assets/installer/sidebar.bmp',ROOT/'assets/installer/AppIcon.ico',ROOT/'releases/Tappyweb-0.3.1-Windows-x64-Setup.exe',makensis,nsis_dir)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--toolchain-license', type=Path)
    parser.add_argument('--updater', type=Path, required=True)
    parser.add_argument('--path-helper', type=Path, help='Also build the NSIS installer')
    parser.add_argument('--makensis', default='makensis')
    parser.add_argument('--nsis-dir', type=Path)
    args = parser.parse_args()
    require_windows_x64(args.binary)
    require_windows_x64(args.updater)
    output = ROOT / 'releases/Tappyweb-0.3.1-windows-x64.zip'
    output.parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='tappy-windows-') as folder:
        stage = Path(folder)
        shutil.copy2(args.binary, stage / 'tappy.exe')
        shutil.copy2(args.updater,stage/'tappy-update.exe')
        (stage/'update-enabled').write_text('0.3.1\n')
        shutil.copy2(ROOT / 'tools/install_windows.ps1', stage / 'install.ps1')
        shutil.copy2(ROOT / 'docs/windows.md', stage / 'START-HERE.md')
        shutil.copy2(ROOT / 'README.md', stage / 'README.md')
        for name in ('examples', 'docs'):
            shutil.copytree(ROOT / name, stage / name, ignore=shutil.ignore_patterns('.tappy-data', '.DS_Store'))
        (stage / 'licenses').mkdir()
        if args.toolchain_license: shutil.copy2(args.toolchain_license, stage / 'licenses/llvm-mingw.txt')
        shutil.copy2(ROOT / 'vendor/httplib.LICENSE', stage / 'licenses/cpp-httplib.txt')
        shutil.copy2(ROOT / 'assets/htmx.LICENSE', stage / 'licenses/htmx.txt')
        shutil.copy2(ROOT/'vendor/json.LICENSE',stage/'licenses/json.txt')
        with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(stage.rglob('*')):
                if path.is_file(): archive.write(path, path.relative_to(stage))
    print(output)
    if args.path_helper:
        build_installer(args.path_helper, args.makensis, args.nsis_dir)

if __name__ == '__main__':
    main()
