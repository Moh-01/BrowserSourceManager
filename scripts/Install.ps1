<#
.SYNOPSIS
  Installs or removes Browser Source Manager for OBS Studio on Windows.

.DESCRIPTION
  By default it:
    1. downloads the latest release from GitHub,
    2. extracts it into %ProgramData%\obs-studio\plugins\browser-source-manager
       (where OBS 30 and newer load plugins from),
    3. replaces any older version that is already there.
  Close OBS first.

.EXAMPLE
  .\Install.ps1                      # download and install the latest release
  .\Install.ps1 -Zip .\browser-source-manager-1.0.1-windows-x64.zip
  .\Install.ps1 -Uninstall           # remove the plugin
  .\Install.ps1 -FromBuild           # install your own build from build_x64 (developers)
#>
param(
    [string]$Zip,
    [switch]$FromBuild,
    [string]$Config = 'RelWithDebInfo',
    [switch]$Uninstall,
    [string]$Destination = (Join-Path $env:ProgramData 'obs-studio\plugins')
)

$ErrorActionPreference = 'Stop'
$name = 'browser-source-manager'
$repo = 'Moh-01/BrowserSourceManager'
$target = Join-Path $Destination $name
$work = Join-Path ([IO.Path]::GetTempPath()) "$name-install"

# Started from "Run with PowerShell": keep the window open at the end so the result can be read.
$ownWindow = $PSCommandPath -and (([Environment]::GetCommandLineArgs() -join ' ') -like "*$PSCommandPath*")

try {
    $isObsFolder = $Destination -eq (Join-Path $env:ProgramData 'obs-studio\plugins')
    if ($isObsFolder -and (Get-Process obs64 -ErrorAction SilentlyContinue)) {
        throw 'OBS is running. Close OBS, then run this again.'
    }

    if ($Uninstall) {
        if (Test-Path $target) {
            Remove-Item $target -Recurse -Force
            Write-Host "Removed Browser Source Manager from $target"
        } else {
            Write-Host 'Browser Source Manager is not installed.'
        }
        return
    }

    if (Test-Path $work) { Remove-Item $work -Recurse -Force }
    New-Item -ItemType Directory $work | Out-Null

    if ($FromBuild) {
        if (-not $PSScriptRoot) { throw '-FromBuild only works when running the script from a clone of the repo.' }
        $source = Join-Path (Split-Path -Parent $PSScriptRoot) "build_x64\rundir\$Config\$name"
        if (-not (Test-Path $source)) { throw "No build found at $source. Build the plugin first." }
    } else {
        if (-not $Zip) {
            [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
            Write-Host 'Looking up the latest release...'
            $release = Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest" -Headers @{ 'User-Agent' = $name }
            $asset = $release.assets | Where-Object { $_.name -like "$name-*-windows-x64.zip" } | Select-Object -First 1
            if (-not $asset) { throw "No Windows download found in release $($release.tag_name)." }
            $Zip = Join-Path $work $asset.name
            Write-Host "Downloading $($asset.name)..."
            Invoke-WebRequest $asset.browser_download_url -OutFile $Zip -UseBasicParsing
        }
        if (-not (Test-Path $Zip)) { throw "Zip not found: $Zip" }
        Expand-Archive -Path $Zip -DestinationPath $work -Force
        $source = Join-Path $work $name
        if (-not (Test-Path (Join-Path $source "bin\64bit\$name.dll"))) { throw 'The zip does not contain the plugin files.' }
    }

    if (Test-Path $target) { Remove-Item $target -Recurse -Force }
    New-Item -ItemType Directory -Force $target | Out-Null
    Copy-Item (Join-Path $source '*') $target -Recurse -Force
    Write-Host "Installed Browser Source Manager to $target" -ForegroundColor Green
    Write-Host 'Start OBS and open Docks > Browser Sources.'
} catch {
    Write-Host "Install failed: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
    if ($ownWindow) { Read-Host 'Press Enter to close' | Out-Null }
}
