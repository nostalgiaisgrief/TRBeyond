param([string]$Python='python')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'tools/build-version.ps1')
$setupName = "TombRaiderBeyond-$releaseVersion"
# Reject a stale game binary rather than bundling mismatched versions.
$runtime = Join-Path $PSScriptRoot 'build/tomb_preview.exe'
if (-not (Test-Path -LiteralPath $runtime) -or (Get-Item -LiteralPath $runtime).VersionInfo.ProductVersion -ne $releaseVersion) {
    throw 'Build the current game version first: ./build-trbeyond.ps1 -BuildOnly'
}
Push-Location $PSScriptRoot
try {
    $env:PYTHONPATH=(Join-Path $PSScriptRoot 'work/python-deps')
    $payload=Join-Path $PSScriptRoot 'build/setup-bundle/payload'
    New-Item -ItemType Directory -Path $payload -Force | Out-Null
    Copy-Item -LiteralPath build/tomb_preview.exe -Destination "$payload/TombRaiderBeyond.exe" -Force
    $legacyPayload = Join-Path $payload 'Tomb Raider Beyond.exe'
    if (Test-Path -LiteralPath $legacyPayload) { Remove-Item -LiteralPath $legacyPayload }
    Copy-Item -LiteralPath third_party/openxr/bin/openxr_loader.dll -Destination "$payload/openxr_loader.dll" -Force
    Copy-Item -LiteralPath third_party/openxr/LICENSE -Destination "$payload/OPENXR-LICENSE.txt" -Force
    & $Python tools/prepare_ub.py data_reference_files/UB/DATA "$payload/Source/UB"
    if ($LASTEXITCODE -ne 0) { throw 'Unfinished Business asset preparation failed' }
    # Remove documentation staged by earlier builds; only runtime files ship.
    Get-ChildItem -LiteralPath $payload -File -Filter '*.md' | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName
    }
    $setupVersionFile = Join-Path $PSScriptRoot 'build/setup-bundle/version-info.txt'
    $releaseVersionFile = Join-Path $PSScriptRoot 'version.txt'
    @"
VSVersionInfo(
 ffi=FixedFileInfo(filevers=($resourceVersion), prodvers=($resourceVersion), mask=0x3f, flags=0, OS=0x40004, fileType=1, subtype=0, date=(0,0)),
 kids=[StringFileInfo([StringTable('040904B0', [
 StringStruct('FileDescription', 'Tomb Raider Beyond Installer'),
 StringStruct('ProductName', 'Tomb Raider Beyond'),
 StringStruct('FileVersion', '$fileVersion'),
 StringStruct('ProductVersion', '$releaseVersion'),
 StringStruct('OriginalFilename', '$setupName.exe')])]),
 VarFileInfo([VarStruct('Translation', [1033,1200])])])
"@ | Set-Content -LiteralPath $setupVersionFile -Encoding utf8
    # PyInstaller logs INFO messages to stderr. Windows PowerShell 5.1 can
    # turn redirected stderr into terminating errors under Stop. Let Python
    # capture both streams directly, preserving the actual build exit code.
    $buildRunner = "import subprocess, sys; log = open(sys.argv[1], 'wb'); result = subprocess.run([sys.executable] + sys.argv[2:], stdout=log, stderr=subprocess.STDOUT); log.close(); sys.exit(result.returncode)"
    & $Python -c $buildRunner build/setup-build.log -m PyInstaller --noconfirm --clean --onefile --windowed --name $setupName --version-file $setupVersionFile --distpath dist/setup --workpath build/setup-pyinstaller --specpath build/setup-bundle --paths tools --collect-all av --copy-metadata av --add-data "$payload;payload" --add-data "$releaseVersionFile;." tools/setup_wizard.py
    if($LASTEXITCODE -ne 0){throw 'Wizard build failed; see build/setup-build.log'}
    Get-FileHash -LiteralPath "dist/setup/$setupName.exe" -Algorithm SHA256
} finally {Pop-Location}
