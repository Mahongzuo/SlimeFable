[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string[]]$RecordedCache,

    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$ShaderStableKeys,

    [string]$Project = 'E:\UE\SlimeFable\SlimeFable.uproject',
    [string]$EngineRoot = 'D:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectPath = (Resolve-Path -LiteralPath $Project).Path
$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe not found: $editorCmd"
}

$recordings = foreach ($cache in $RecordedCache) {
    if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) {
        throw "Recorded PSO cache not found: $cache"
    }
    (Resolve-Path -LiteralPath $cache).Path
}
$shaderKeys = (Resolve-Path -LiteralPath $ShaderStableKeys).Path
$pipelineDirectory = Join-Path (Split-Path -Parent $projectPath) 'Build\Windows\PipelineCaches'
New-Item -ItemType Directory -Force -Path $pipelineDirectory | Out-Null

# UE 5.8 Cook scans this naming pattern and builds the packaged cache from it.
$stableCsv = Join-Path $pipelineDirectory 'SlimeFable_PCD3D_SM6.stablepc.csv'
$stableCache = Join-Path $pipelineDirectory 'SlimeFable_PCD3D_SM6.stable.upipelinecache'
Remove-Item -LiteralPath $stableCsv -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $stableCache -Force -ErrorAction SilentlyContinue

$expandArgs = @($projectPath, '-run=ShaderPipelineCacheTools', 'expand') +
    @($recordings) + @($shaderKeys, $stableCsv, '-unattended', '-nop4', '-nullrhi', '-DDC-ForceMemoryCache')
& $editorCmd @expandArgs
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $stableCsv -PathType Leaf)) {
    throw "ShaderPipelineCacheTools expand failed or did not create $stableCsv"
}

$buildArgs = @(
    $projectPath,
    '-run=ShaderPipelineCacheTools',
    'build',
    $stableCsv,
    $shaderKeys,
    $stableCache,
    '-unattended',
    '-nop4',
    '-nullrhi',
    '-DDC-ForceMemoryCache'
)
& $editorCmd @buildArgs
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $stableCache -PathType Leaf)) {
    throw "ShaderPipelineCacheTools build failed or did not create $stableCache"
}

if ((Get-Item -LiteralPath $stableCsv).Length -le 0 -or (Get-Item -LiteralPath $stableCache).Length -le 0) {
    throw 'PSO output exists but is empty.'
}

Write-Host "Cook input: $stableCsv"
Write-Host "Validated bundled cache: $stableCache"
