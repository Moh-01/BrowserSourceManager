param(
    [string]$Zip,
    [switch]$FromBuild,
    [string]$Config = 'RelWithDebInfo',
    [switch]$Uninstall,
    [string]$Destination = (Join-Path $env:ProgramData 'obs-studio\plugins')
)

$ErrorActionPreference = 'Stop'
# Edit these three to reuse this script for another plugin. If you change them, it downloads that repo's latest release,
# installs it into %ProgramData%\obs-studio\plugins\<name>, and shows the new title in its messages.
$name = 'browser-source-manager'
$repo = 'Moh-01/BrowserSourceManager'
$title = 'Browser Source Manager'
$target = Join-Path $Destination $name
$work = Join-Path ([IO.Path]::GetTempPath()) "$name-install"

$ownWindow = $PSCommandPath -and (([Environment]::GetCommandLineArgs() -join ' ') -like "*$PSCommandPath*")

try {
    $isObsFolder = $Destination -eq (Join-Path $env:ProgramData 'obs-studio\plugins')
    if ($isObsFolder -and (Get-Process obs64 -ErrorAction SilentlyContinue)) {
        throw 'OBS is running. Close OBS, then run this again.'
    }

    if ($Uninstall) {
        if (Test-Path $target) {
            Remove-Item $target -Recurse -Force
            Write-Host "Removed $title from $target"
        } else {
            Write-Host "$title is not installed."
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
    Write-Host "Installed $title to $target" -ForegroundColor Green
    Write-Host "Start OBS and open it from the Docks menu."
} catch {
    Write-Host "Install failed: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
    if ($ownWindow) { Read-Host 'Press Enter to close' | Out-Null }
}
