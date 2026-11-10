param([Parameter(Mandatory=$true)][string]$Helper)
$ErrorActionPreference = 'Stop'
$Original = [Environment]::GetEnvironmentVariable('Path', 'User')
$Entry = Join-Path $env:TEMP 'Tappyweb PATH test Юникод'
try {
    $LongPath = ((1..150 | ForEach-Object { "C:\Test-Existing-Path-Entry-Number-$_" }) -join ';')
    [Environment]::SetEnvironmentVariable('Path', $LongPath, 'User')
    & $Helper add $Entry
    if ($LASTEXITCODE -ne 0) { throw 'PATH add failed' }
    $Added = [Environment]::GetEnvironmentVariable('Path', 'User')
    if ($Added -ne "$LongPath;$Entry") { throw 'Existing long PATH was modified or truncated' }
    & $Helper add $Entry
    if ($LASTEXITCODE -ne 10) { throw 'Duplicate entry was not detected' }
    & $Helper remove $Entry
    if ($LASTEXITCODE -ne 0) { throw 'PATH removal failed' }
    if ([Environment]::GetEnvironmentVariable('Path', 'User') -ne $LongPath) { throw 'PATH was not restored exactly' }
    Write-Output 'Long PATH, duplicate detection, Unicode/spaces, and removal tests passed.'
} finally {
    [Environment]::SetEnvironmentVariable('Path', $Original, 'User')
}
