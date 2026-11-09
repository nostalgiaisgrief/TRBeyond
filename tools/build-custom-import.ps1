param([Parameter(Mandatory)][string]$Python)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $env:PYTHONPATH = Join-Path $root 'work/python-deps'
    $runner = "import subprocess,sys; log=open('build/custom-import-build.log','wb'); sys.exit(subprocess.run([sys.executable]+sys.argv[1:],stdout=log,stderr=subprocess.STDOUT).returncode)"
    & $Python -c $runner -m PyInstaller --noconfirm --onefile --console --name TRCustomImport --distpath build --workpath build/custom-pyinstaller --specpath build --collect-all av --copy-metadata av tools/custom_import.py
    if ($LASTEXITCODE -ne 0) { throw 'Custom level importer build failed. See build/custom-import-build.log.' }
} finally { Pop-Location }
