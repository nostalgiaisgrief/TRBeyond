param(
    [string]$Python = 'C:\Users\richa\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
)

$ErrorActionPreference = 'Stop'
# Launch with powershell -ExecutionPolicy Bypass -File .\build-release.ps1
# so this file can start even when the normal policy blocks scripts.
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force

if (-not (Test-Path -LiteralPath $Python -PathType Leaf)) {
    throw "Python was not found at '$Python'. Supply its full path with -Python."
}

Push-Location $PSScriptRoot
try {
    Write-Host 'Building Tomb Raider Beyond...'
    & (Join-Path $PSScriptRoot 'build-trbeyond.ps1') -BuildOnly

    Write-Host 'Building the installer...'
    & (Join-Path $PSScriptRoot 'build-setup.ps1') -Python $Python

    $version = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'version.txt') -Raw).Trim()
    $installer = Join-Path $PSScriptRoot "dist/setup/TombRaiderBeyond-$version.exe"
    if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
        throw "The installer was not created: $installer"
    }
    Write-Host "Release ready: $installer"
} finally {
    Pop-Location
}
