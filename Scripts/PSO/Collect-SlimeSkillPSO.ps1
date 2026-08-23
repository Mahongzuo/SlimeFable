[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$PackagedExe,

    [string]$OutputDirectory = "$(Split-Path -Parent $PSScriptRoot)\..\Saved\PSO\Recordings"
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $PackagedExe).Path
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $output | Out-Null

$projectSaved = Join-Path (Split-Path -Parent (Split-Path -Parent $exe)) 'SlimeFable\Saved\CollectedPSOs'
$localSaved = Join-Path $env:LOCALAPPDATA 'SlimeFable\Saved\CollectedPSOs'
$roots = @($projectSaved, $localSaved) | Select-Object -Unique
$startedAt = Get-Date

Write-Host 'Launching the packaged game in PSO recording mode.'
Write-Host 'Exercise all 18 skills, six combo finishers, projectile impacts, and element reactions, then exit normally.'
$arguments = @(
    '-logPSO',
    '-clearPSODriverCache',
    '-log',
    '-ExecCmds=r.ShaderPipelineCache.LogPSO 1'
)
$process = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -Wait
if ($process.ExitCode -ne 0) {
    throw "Packaged game exited with code $($process.ExitCode)."
}

$recordings = foreach ($root in $roots) {
    if (Test-Path -LiteralPath $root) {
        Get-ChildItem -LiteralPath $root -Filter '*.rec.upipelinecache' -File |
            Where-Object { $_.LastWriteTime -ge $startedAt }
    }
}
$recordings = @($recordings | Sort-Object FullName -Unique)
if ($recordings.Count -eq 0) {
    throw "No new *.rec.upipelinecache was found under: $($roots -join ', ')"
}

foreach ($recording in $recordings) {
    $destination = Join-Path $output $recording.Name
    Copy-Item -LiteralPath $recording.FullName -Destination $destination -Force
    Write-Host "Collected $destination"
}
