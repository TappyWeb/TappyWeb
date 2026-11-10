#!/usr/bin/env python3
"""Build a universal, self-contained macOS CLI installer without installing it."""
import os
from pathlib import Path
import platform
import plistlib
import zipfile
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
VERSION = '0.3.1'
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
    binary = root / 'usr/local/libexec/tappyweb/tappy'
    binary.parent.mkdir(parents=True)
    print('Building for Apple Silicon and Intel (macOS 13+).', flush=True)
    run('clang++', '-std=c++20', '-O2', '-pthread', '-arch', 'arm64', '-arch', 'x86_64',
        '-mmacosx-version-min=13.0', '-Ivendor', 'src/main.cpp', '-o', str(binary))
    helper = binary.with_name('tappy-update')
    run('clang++', '-std=c++20', '-O2', '-arch', 'arm64', '-arch', 'x86_64',
        '-mmacosx-version-min=13.0', '-Ivendor', 'tools/updater.cpp', '-o', str(helper))
    helper.chmod(0o755)
    run('codesign', '--force', '--sign', '-', str(helper))
    (binary.parent/'update-enabled').write_text(VERSION+'\n')
    wrapper = root/'usr/local/bin/tappy';wrapper.parent.mkdir(parents=True)
    wrapper.write_text('#!/bin/sh\nexec /usr/local/libexec/tappyweb/tappy "$@"\n');wrapper.chmod(0o755)
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
    shutil.copy2(ROOT / 'vendor/json.LICENSE', licenses / 'json.txt')
    # macOS path_helper reads this for new login shells, including Terminal's default zsh.
    paths = root / 'etc/paths.d'
    paths.mkdir(parents=True)
    (paths / 'tappyweb').write_text('/usr/local/bin\n')
    scripts = Path(tmp) / 'scripts'
    scripts.mkdir()
    postinstall = scripts / 'postinstall'
    app = root/'Applications/Tappyweb.app'; appbin=app/'Contents/MacOS';appbin.mkdir(parents=True)
    resources=app/'Contents/Resources';resources.mkdir()
    slices=[]
    for arch in ('arm64','x86_64'):
        target=Path(tmp)/('launcher-'+arch)
        run('swiftc','-O','-target',arch+'-apple-macosx13.0','-framework','AppKit','-framework','CoreServices','tools/macos_launcher.swift','-o',str(target));slices.append(str(target))
    run('lipo','-create',*slices,'-output',str(appbin/'TappywebLauncher'))
    iconset=Path(tmp)/'Tappy.iconset';iconset.mkdir()
    for size in (16,32,128,256,512):
        for scale in (1,2):
            suffix='@2x' if scale==2 else ''
            run('sips','-z',str(size*scale),str(size*scale),str(ROOT/'assets/installer/Logo.png'),'--out',str(iconset/f'icon_{size}x{size}{suffix}.png'))
    run('iconutil','-c','icns',str(iconset),'-o',str(resources/'AppIcon.icns'))
    info={'CFBundleExecutable':'TappywebLauncher','CFBundleIdentifier':'dev.tappyweb.launcher','CFBundleName':'Tappyweb','CFBundlePackageType':'APPL','CFBundleVersion':VERSION,'CFBundleShortVersionString':VERSION,'CFBundleIconFile':'AppIcon.icns','LSMinimumSystemVersion':'13.0','LSUIElement':True,
      'CFBundleDocumentTypes':[{'CFBundleTypeName':'Tappyweb source','CFBundleTypeRole':'Viewer','LSHandlerRank':'Owner','LSItemContentTypes':['dev.tappyweb.source']}],
      'UTExportedTypeDeclarations':[{'UTTypeIdentifier':'dev.tappyweb.source','UTTypeDescription':'Tappyweb source','UTTypeConformsTo':['public.plain-text'],'UTTypeTagSpecification':{'public.filename-extension':['tappy']}}]}
    (app/'Contents/Info.plist').write_bytes(plistlib.dumps(info));run('codesign','--force','--sign','-',str(app))
    postinstall.write_text('''#!/bin/sh
set -eu
TARGET_VOLUME="${3:-/}"
/bin/chmod 755 "$TARGET_VOLUME/usr/local/bin/tappy" "$TARGET_VOLUME/usr/local/libexec/tappyweb/tappy" "$TARGET_VOLUME/usr/local/libexec/tappyweb/tappy-update"
"$TARGET_VOLUME/usr/local/libexec/tappyweb/tappy" --version >/dev/null
if [ "$TARGET_VOLUME" = / ]; then
    /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f /Applications/Tappyweb.app
    CONSOLE_USER=$(/usr/bin/stat -f%Su /dev/console)
    if [ "$CONSOLE_USER" != root ] && [ "$CONSOLE_USER" != loginwindow ]; then
        CONSOLE_UID=$(/usr/bin/id -u "$CONSOLE_USER")
        /bin/launchctl asuser "$CONSOLE_UID" /usr/bin/sudo -u "$CONSOLE_USER" /Applications/Tappyweb.app/Contents/MacOS/TappywebLauncher --register || true
    fi
fi
exit 0
''')
    portable=ROOT/'releases'/f'Tappyweb-{VERSION}-macOS-universal.zip'
    with zipfile.ZipFile(portable,'w',zipfile.ZIP_DEFLATED) as archive:
        for file in binary.parent.iterdir():archive.write(file,file.name)
        archive.write(ROOT/'vendor/json.LICENSE','licenses/json.txt')
        archive.write(ROOT/'vendor/httplib.LICENSE','licenses/cpp-httplib.txt')
        archive.write(ROOT/'assets/htmx.LICENSE','licenses/htmx.txt')
    postinstall.chmod(0o755)
    run('pkgbuild', '--root', str(root), '--identifier', IDENTIFIER, '--version', VERSION,
        '--install-location', '/', '--ownership', 'recommended', '--scripts', str(scripts), str(output))
print(f'Installer: {output}')
print('Installs tappy to /usr/local/bin; requires no compiler or Python on the recipient Mac.')
