#!/usr/bin/env python3
"""Build a universal, self-contained macOS CLI installer without installing it."""
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
VERSION = '0.2.0'
IDENTIFIER = 'dev.tappyweb.cli'

def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)

if platform.system() != 'Darwin':
    raise SystemExit('macOS packaging requires macOS and Apple Command Line Tools.')
for tool in ('clang++', 'pkgbuild', 'lipo', 'codesign'):
    if not shutil.which(tool):
        raise SystemExit(f'Missing {tool}. Install Apple Command Line Tools with xcode-select --install.')

output = ROOT / 'releases' / f'Tappyweb-{VERSION}-macOS-universal.pkg'
output.parent.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='tappy-pkg-') as tmp:
    root = Path(tmp) / 'root'
    binary = root / 'usr/local/bin/tappy'
    binary.parent.mkdir(parents=True)
    print('Building for Apple Silicon and Intel (macOS 13+).', flush=True)
    run('clang++', '-std=c++20', '-O2', '-pthread', '-arch', 'arm64', '-arch', 'x86_64',
        '-mmacosx-version-min=13.0', '-Ivendor', 'src/main.cpp', '-o', str(binary))
    binary.chmod(0o755)
    # Ad-hoc signing allows local execution; distribution signing is a separate step.
    run('codesign', '--force', '--sign', '-', str(binary))
    run('lipo', '-info', str(binary))
    run('codesign', '--verify', '--strict', str(binary))
    run(str(binary), '--version')
    share = root / 'usr/local/share/tappyweb'
    shutil.copytree(ROOT / 'examples', share / 'examples', ignore=shutil.ignore_patterns('.tappy-data'))
    shutil.copytree(ROOT / 'docs', share / 'docs')
    shutil.copy2(ROOT / 'README.md', share / 'README.md')
    licenses = share / 'licenses'
    licenses.mkdir()
    shutil.copy2(ROOT / 'vendor/httplib.LICENSE', licenses / 'cpp-httplib.txt')
    shutil.copy2(ROOT / 'assets/htmx.LICENSE', licenses / 'htmx.txt')
    # macOS path_helper reads this for new login shells, including Terminal's default zsh.
    paths = root / 'etc/paths.d'
    paths.mkdir(parents=True)
    (paths / 'tappyweb').write_text('/usr/local/bin\n')
    scripts = Path(tmp) / 'scripts'
    scripts.mkdir()
    postinstall = scripts / 'postinstall'
    postinstall.write_text('#!/bin/sh\nset -eu\nTARGET_VOLUME="${3:-/}"\n/bin/chmod 755 "$TARGET_VOLUME/usr/local/bin/tappy"\n"$TARGET_VOLUME/usr/local/bin/tappy" --version >/dev/null\nexit 0\n')
    postinstall.chmod(0o755)
    run('pkgbuild', '--root', str(root), '--identifier', IDENTIFIER, '--version', VERSION,
        '--install-location', '/', '--ownership', 'recommended', '--scripts', str(scripts), str(output))
print(f'Installer: {output}')
print('Installs tappy to /usr/local/bin; requires no compiler or Python on the recipient Mac.')
