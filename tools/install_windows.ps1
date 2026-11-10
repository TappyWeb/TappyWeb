$ErrorActionPreference = 'Stop'
$Destination = Join-Path $env:LOCALAPPDATA 'Programs\Tappyweb'
New-Item -ItemType Directory -Force $Destination | Out-Null
if ([IO.Path]::GetFullPath($PSScriptRoot) -ne [IO.Path]::GetFullPath($Destination)) {
    Get-ChildItem $PSScriptRoot | Copy-Item -Destination $Destination -Recurse -Force
}
$UserPath = [Environment]::GetEnvironmentVariable('Path','User')
if (!$UserPath) { $UserPath = '' }
if (@($UserPath -split ';') -notcontains $Destination) {
    [Environment]::SetEnvironmentVariable('Path', (($UserPath.TrimEnd(';') + ';' + $Destination).TrimStart(';')), 'User')
}
Write-Output "Installed to $Destination. Open a new terminal, then run: tappy file.tappy"
