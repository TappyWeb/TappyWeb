param([string]$Binary = '', [string]$Architecture = 'x64', [string]$PathHelper = '')
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
$Stage = "$Root/build/package-$Architecture"
New-Item -ItemType Directory -Force $Stage | Out-Null
Copy-Item $Binary "$Stage/tappy.exe" -Force
foreach ($Folder in @('examples','docs')) {
    if (Test-Path "$Stage/$Folder") { Remove-Item "$Stage/$Folder" -Recurse -Force }
    New-Item -ItemType Directory -Force "$Stage/$Folder" | Out-Null
    Get-ChildItem "$Root/$Folder" -File | Copy-Item -Destination "$Stage/$Folder"
}
Copy-Item "$Root/tools/install_windows.ps1" "$Stage/install.ps1" -Force
Copy-Item "$Root/README.md" $Stage -Force
Copy-Item "$Root/docs/windows.md" "$Stage/START-HERE.md" -Force
New-Item -ItemType Directory -Force "$Stage/licenses" | Out-Null
Copy-Item "$Root/vendor/httplib.LICENSE" "$Stage/licenses/cpp-httplib.txt" -Force
Copy-Item "$Root/assets/htmx.LICENSE" "$Stage/licenses/htmx.txt" -Force
New-Item -ItemType Directory -Force "$Root/releases" | Out-Null
$Zip = "$Root/releases/Tappyweb-0.2.0-windows-$Architecture.zip"
Compress-Archive -Path "$Stage/*" -DestinationPath $Zip -Force
Write-Output $Zip

$NSIS = (Get-Command makensis -ErrorAction SilentlyContinue).Source
if (!$NSIS) { $NSIS = Join-Path ${env:ProgramFiles(x86)} 'NSIS/makensis.exe' }
if (!(Test-Path $NSIS)) { throw 'Install NSIS 3 to build the Setup.exe installer.' }
python "$Root/tools/build_installer.py" --path-helper $PathHelper --makensis $NSIS
if ($LASTEXITCODE) { throw 'NSIS installer build failed' }
