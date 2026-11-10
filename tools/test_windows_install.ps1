$ErrorActionPreference = 'Stop'
$installer = (Get-ChildItem "$PSScriptRoot/../releases/*Windows-x64-Setup.exe" | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
$target = Join-Path $env:TEMP 'Tappy installation Ω'
$association = 'HKCU:\Software\Classes\.tappy'
New-Item $association -Force | Out-Null
Set-Item $association -Value 'Tappy.Test.Previous'
$process = Start-Process $installer -ArgumentList @('/S', "/D=$target") -Wait -PassThru
if ($process.ExitCode) { throw "Install failed: $($process.ExitCode)" }
foreach ($name in @('tappy.exe','tappy-update.exe','update-enabled','Uninstall.exe')) {
 if (!(Test-Path (Join-Path $target $name))) { throw "Missing $name" }
}
if ((Get-Item $association).GetValue('') -ne 'Tappyweb.Source') { throw 'Association missing' }
$command = (Get-Item 'HKCU:\Software\Classes\Tappyweb.Source\shell\open\command').GetValue('')
if (!$command.Contains('cmd.exe" /d /s /k') -or !$command.Contains('tappy.exe" "%1"')) { throw "Invalid open command: $command" }
& (Join-Path $target 'tappy.exe') --version
if ($LASTEXITCODE) { throw 'Installed executable failed' }
$uninstaller = Join-Path $target 'Uninstall.exe'
$process = Start-Process $uninstaller -ArgumentList @('/S', "_?=$target") -Wait -PassThru
if ($process.ExitCode) { throw 'Uninstall failed' }
if ((Get-Item $association).GetValue('') -ne 'Tappy.Test.Previous') { throw 'Previous association was not restored' }
Remove-Item $association -Recurse -Force
Write-Output 'Install, command association, runtime and uninstall checks passed.'
