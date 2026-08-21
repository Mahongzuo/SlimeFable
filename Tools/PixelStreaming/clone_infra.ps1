# Clone official UE5.8 Pixel Streaming infra (gitignored). Uses a GitHub proxy if github.com is blocked.
$ErrorActionPreference = "Stop"
$dest = Join-Path $PSScriptRoot "PixelStreamingInfrastructure"
if (Test-Path $dest) {
    Write-Host "Already present: $dest"
    exit 0
}
$urls = @(
    "https://ghproxy.net/https://github.com/EpicGames/PixelStreamingInfrastructure.git",
    "https://github.com/EpicGames/PixelStreamingInfrastructure.git",
    "https://github.com/EpicGamesExt/PixelStreamingInfrastructure.git"
)
foreach ($url in $urls) {
    Write-Host "Cloning UE5.8 from $url"
    git -c http.version=HTTP/1.1 clone --branch UE5.8 --depth 1 $url $dest
    if ($LASTEXITCODE -eq 0) { exit 0 }
}
Write-Error "Clone failed. Check network or run the git clone yourself."
