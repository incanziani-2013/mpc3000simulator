# Usage (PowerShell): .\install_library.ps1 -Source "C:\path\to\MPC 3000 Kit\MPC3000"
# Copies the library once to a stable location the plugin searches automatically.
# The original ZIP is not modified. The plugin stores library-relative paths in FL Studio projects.
param([Parameter(Mandatory=$true)][string]$Source)
$dest = Join-Path $env:ProgramData "MPC3000 SIMULATOR\Library"
if (-not (Test-Path $Source)) { throw "Source not found: $Source" }
New-Item -ItemType Directory -Force -Path $dest | Out-Null
robocopy $Source $dest /E /XF *.MID | Out-Null
Write-Host "Library installed to $dest"
