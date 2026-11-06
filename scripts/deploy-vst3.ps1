<#
.SYNOPSIS
    Copies the built RipplerX VST3 bundle into the system VST3 folder.

.DESCRIPTION
    Writing to "C:\Program Files\Common Files\VST3" needs administrator rights, so the
    script relaunches itself elevated (UAC prompt) when it isn't already.
    Progress and errors are appended to build\deploy-vst3.log.

.PARAMETER Config
    Build configuration to deploy (Release or Debug). Defaults to Release.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\deploy-vst3.ps1
    powershell -ExecutionPolicy Bypass -File scripts\deploy-vst3.ps1 -Config Debug
#>
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repoRoot "build\RipplerX_artefacts\$Config\VST3\RipplerX.vst3"
$destDir = Join-Path $env:CommonProgramFiles 'VST3'
$dest = Join-Path $destDir 'RipplerX.vst3'
$logFile = Join-Path $repoRoot 'build\deploy-vst3.log'

function Write-Log([string]$message) {
    $line = "[{0}] {1}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $message
    Write-Host $line
    try { Add-Content -Path $logFile -Value $line -Encoding utf8 } catch {}
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Log "Requesting administrator rights to deploy $Config VST3..."
    $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Config $Config"
    try {
        Start-Process -FilePath 'powershell.exe' -ArgumentList $arguments -Verb RunAs
    }
    catch {
        Write-Log "ERROR: elevation was cancelled or failed: $($_.Exception.Message)"
        exit 1
    }
    exit 0
}

try {
    if (-not (Test-Path $source)) {
        throw "Build output not found: $source (build the RipplerX_VST3 target first)"
    }

    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    }

    if (Test-Path $dest) {
        Remove-Item -Recurse -Force -Path $dest
    }

    Copy-Item -Recurse -Force -Path $source -Destination $dest
    Write-Log "Deployed $Config VST3 to $dest"
}
catch {
    Write-Log "ERROR: $($_.Exception.Message)"
    Write-Log "If a DAW has the plugin loaded, close it and run the script again."
    Start-Sleep -Seconds 10 # leave the elevated window up long enough to read the error
    exit 1
}
