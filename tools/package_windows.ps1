param([string]$Binary = '', [ValidateSet('x64')][string]$Architecture = 'x64', [string]$PathHelper = '')
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
if (!$Binary -or !$PathHelper) {
    cmake -S $Root -B "$Root/build/windows-$Architecture" -A $Architecture
    if ($LASTEXITCODE) { throw 'CMake configuration failed' }
    cmake --build "$Root/build/windows-$Architecture" --config Release
    if ($LASTEXITCODE) { throw 'CLI build failed' }
    if (!$Binary) { $Binary = "$Root/build/windows-$Architecture/Release/tappy.exe" }
    if (!$PathHelper) { $PathHelper = "$Root/build/windows-$Architecture/Release/tappy-env.exe" }
}
$NSIS = (Get-Command makensis -ErrorAction SilentlyContinue).Source
if (!$NSIS) { $NSIS = Join-Path ${env:ProgramFiles(x86)} 'NSIS/makensis.exe' }
if (!(Test-Path $NSIS)) { throw 'Install NSIS 3 to build the Setup.exe installer.' }
python "$Root/tools/package_windows.py" --binary $Binary --path-helper $PathHelper --makensis $NSIS
if ($LASTEXITCODE) { throw 'Windows packaging failed' }
