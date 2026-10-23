# Shared version reader for the game and setup builds.
$projectRoot = Split-Path $PSScriptRoot -Parent
$releaseVersion = (Get-Content -LiteralPath (Join-Path $projectRoot 'version.txt') -Raw).Trim()
if ($releaseVersion -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
    throw 'version.txt must contain a version such as 0.0.3 (major.minor.patch).'
}
$versionParts = @($releaseVersion.Split('.') | ForEach-Object { [int]$_ }) + @(0)
if (@($versionParts | Where-Object { $_ -gt 65535 }).Count) {
    throw 'Each version component must be between 0 and 65535.'
}
$fileVersion = $versionParts -join '.'
$resourceVersion = $versionParts -join ','
