<#
.SYNOPSIS
  Installs (or removes) the built plugin for the OBS Studio installed on this PC.

.DESCRIPTION
  Copies the plugin from build_x64 into %ProgramData%\obs-studio\plugins\browser-source-manager,
  where OBS 30 and newer load third-party plugins from. Close OBS first.

.EXAMPLE
  .\scripts\Install.ps1              # install
  .\scripts\Install.ps1 -Uninstall   # remove
#>
param(
    [string]$Config = 'RelWithDebInfo',
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$name = 'browser-source-manager'
$repo = Split-Path -Parent $PSScriptRoot
$src = Join-Path $repo "build_x64\rundir\$Config\$name"
$dest = Join-Path $env:ProgramData "obs-studio\plugins\$name"

if (Get-Process obs64 -ErrorAction SilentlyContinue) {
    throw 'OBS is running. Close it first.'
}

if ($Uninstall) {
    if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
    Write-Host "Removed $name"
    return
}

if (-not (Test-Path $src)) {
    throw "Not built yet. Run: cmake --build --preset windows-x64"
}
New-Item -ItemType Directory -Force $dest | Out-Null
Copy-Item (Join-Path $src '*') $dest -Recurse -Force
Write-Host "Installed $name -> $dest"
