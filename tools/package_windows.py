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

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--toolchain-license', type=Path)
    args = parser.parse_args()
    require_windows_x64(args.binary)
    output = ROOT / 'releases/Tappyweb-0.2.0-windows-x64.zip'
    output.parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='tappy-windows-') as folder:
        stage = Path(folder)
        shutil.copy2(args.binary, stage / 'tappy.exe')
        shutil.copy2(ROOT / 'tools/install_windows.ps1', stage / 'install.ps1')
        shutil.copy2(ROOT / 'docs/windows.md', stage / 'START-HERE.md')
        shutil.copy2(ROOT / 'README.md', stage / 'README.md')
        for name in ('examples', 'docs'):
            shutil.copytree(ROOT / name, stage / name, ignore=shutil.ignore_patterns('.tappy-data', '.DS_Store'))
        (stage / 'licenses').mkdir()
        if args.toolchain_license: shutil.copy2(args.toolchain_license, stage / 'licenses/llvm-mingw.txt')
        shutil.copy2(ROOT / 'vendor/httplib.LICENSE', stage / 'licenses/cpp-httplib.txt')
        shutil.copy2(ROOT / 'assets/htmx.LICENSE', stage / 'licenses/htmx.txt')
        with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(stage.rglob('*')):
                if path.is_file(): archive.write(path, path.relative_to(stage))
    print(output)

if __name__ == '__main__':
    main()
