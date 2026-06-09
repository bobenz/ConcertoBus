<#
.SYNOPSIS
    Assembles a self-contained Windows deployment package for ConcertoBus.

.DESCRIPTION
    Produces two distributable components:
      daemon\          - ConcertoBusDaemon.exe + all required DLLs (Qt, QXmpp, MSVC runtime)
      imports\ConcertoBus\ - QML plugin drop-in for client applications

.PARAMETER BuildDir
    Path to the CMake build output directory (RelWithDebInfo).
    Default: <repo>\build\xmpp-test\RelWithDebInfo

.PARAMETER ImportsDir
    Path to the CMake QML imports output directory.
    Default: <repo>\build\xmpp-test\imports

.PARAMETER OutDir
    Destination directory for the deployment package.
    Default: <repo>\dist\ConcertoBus-1.0-Win64

.PARAMETER QtBin
    Path to the Qt6 MSVC bin directory (contains windeployqt.exe and Qt DLLs).
    Default: D:\Qt\6.10.3\msvc2022_64\bin

.PARAMETER QxmppBin
    Path to the directory containing QXmppQt6.dll.
    Default: D:\Qt\qxmpp-installed\bin

.PARAMETER WithXmpp
    When set, copies QXmppQt6.dll into the daemon directory.
    Default: enabled (pass -WithXmpp:$false to skip)

.EXAMPLE
    .\deploy\Deploy-ConcertoBus.ps1
    # Runs with all defaults; output lands in dist\ConcertoBus-1.0-Win64\

.EXAMPLE
    .\deploy\Deploy-ConcertoBus.ps1 -OutDir C:\Packages\ConcertoBus -WithXmpp:$false
#>

[CmdletBinding()]
param(
    [string] $BuildDir   = (Join-Path $PSScriptRoot '..\build\xmpp-test\RelWithDebInfo'),
    [string] $ImportsDir = (Join-Path $PSScriptRoot '..\build\xmpp-test\imports'),
    [string] $OutDir     = (Join-Path $PSScriptRoot '..\dist\ConcertoBus-1.0-Win64'),
    [string] $QtBin      = 'D:\Qt\6.10.3\msvc2022_64\bin',
    [string] $QxmppBin   = 'D:\Qt\qxmpp-installed\bin',
    [switch] $WithXmpp   = $true,
    # Path to the MSVC toolchain bin\Hostx64\x64 directory containing vcruntime140.dll etc.
    # Auto-detected from a standard VS 2022 install if left empty.
    [string] $MsvcBin    = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolve paths to absolute so all relative defaults work regardless of cwd
$BuildDir   = Resolve-Path $BuildDir
$ImportsDir = Resolve-Path $ImportsDir

$DaemonOut  = Join-Path $OutDir 'daemon'
$ImportsOut = Join-Path $OutDir 'imports\ConcertoBus'

# ── Step 1: Create output directories ────────────────────────────────────────
Write-Host "`n[1/5] Creating output directories..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $DaemonOut  | Out-Null
New-Item -ItemType Directory -Force -Path $ImportsOut | Out-Null
Write-Host "      $DaemonOut"
Write-Host "      $ImportsOut"

# ── Step 2: Copy daemon executable ───────────────────────────────────────────
Write-Host "`n[2/5] Copying ConcertoBusDaemon.exe..." -ForegroundColor Cyan
$DaemonExe = Join-Path $BuildDir 'ConcertoBusDaemon.exe'
if (-not (Test-Path $DaemonExe)) {
    Write-Error "ConcertoBusDaemon.exe not found at: $DaemonExe`nRun the build first."
}
Copy-Item $DaemonExe -Destination $DaemonOut
Write-Host "      OK"

# ── Step 3: Run windeployqt (Qt DLLs + network plugins) ─────────────────────
Write-Host "`n[3/6] Running windeployqt..." -ForegroundColor Cyan
$WinDeploy = Join-Path $QtBin 'windeployqt.exe'
if (-not (Test-Path $WinDeploy)) {
    Write-Error "windeployqt.exe not found at: $WinDeploy`nCheck -QtBin parameter."
}

$DeployTarget = Join-Path $DaemonOut 'ConcertoBusDaemon.exe'
& $WinDeploy `
    --no-quick-import `
    --no-translations `
    --no-system-d3d-compiler `
    --release `
    $DeployTarget

if ($LASTEXITCODE -ne 0) {
    Write-Error "windeployqt failed with exit code $LASTEXITCODE"
}
Write-Host "      OK"

# ── Step 3b: Copy MSVC runtime DLLs ─────────────────────────────────────────
# windeployqt --compiler-runtime requires VCINSTALLDIR which is only set in a
# Developer shell. We copy the required DLLs directly from the toolchain.
Write-Host "`n[3b] Copying MSVC runtime DLLs..." -ForegroundColor Cyan

if ($MsvcBin -eq '') {
    # Auto-detect: find the latest VS 2022 MSVC toolchain
    $vsRoots = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional',
        'C:\Program Files\Microsoft Visual Studio\2022\Community',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools'
    )
    foreach ($vsRoot in $vsRoots) {
        $toolsRoot = Join-Path $vsRoot 'VC\Tools\MSVC'
        if (Test-Path $toolsRoot) {
            $latest = Get-ChildItem $toolsRoot -Directory | Sort-Object Name | Select-Object -Last 1
            $candidate = Join-Path $latest.FullName 'bin\Hostx64\x64'
            if (Test-Path (Join-Path $candidate 'vcruntime140.dll')) {
                $MsvcBin = $candidate
                break
            }
        }
    }
}

if ($MsvcBin -eq '' -or -not (Test-Path $MsvcBin)) {
    Write-Warning "MSVC runtime bin not found - skipping. Install the Visual C++ 2022 Redistributable on the target machine."
} else {
    $runtimeDlls = @(
        'vcruntime140.dll',
        'vcruntime140_1.dll',
        'msvcp140.dll',
        'msvcp140_1.dll',
        'msvcp140_2.dll',
        'msvcp140_atomic_wait.dll',
        'msvcp140_codecvt_ids.dll'
    )
    foreach ($dll in $runtimeDlls) {
        $src = Join-Path $MsvcBin $dll
        if (Test-Path $src) {
            Copy-Item $src -Destination $DaemonOut
            Write-Host "      $dll"
        }
    }
}

# ── Step 4: Copy QXmppQt6.dll ────────────────────────────────────────────────
if ($WithXmpp) {
    Write-Host "`n[4/6] Copying QXmppQt6.dll and Qt6Xml.dll (required by QXmpp)..." -ForegroundColor Cyan
    $QxmppDll = Join-Path $QxmppBin 'QXmppQt6.dll'
    if (-not (Test-Path $QxmppDll)) {
        Write-Warning "QXmppQt6.dll not found at: $QxmppDll - skipping (daemon will fail to start if built with XMPP support)"
    } else {
        Copy-Item $QxmppDll -Destination $DaemonOut
        Write-Host "      QXmppQt6.dll"
    }
    # Qt6Xml.dll is a direct dependency of QXmppQt6.dll but windeployqt
    # doesn't pull it in (daemon itself doesn't use XML directly).
    $Qt6Xml = Join-Path $QtBin 'Qt6Xml.dll'
    if (Test-Path $Qt6Xml) {
        Copy-Item $Qt6Xml -Destination $DaemonOut
        Write-Host "      Qt6Xml.dll"
    } else {
        Write-Warning "Qt6Xml.dll not found in QtBin - QXmpp federation may fail to load"
    }
} else {
    Write-Host "`n[4/6] Skipping QXmppQt6.dll (-WithXmpp:$false)" -ForegroundColor DarkGray
}

# ── Step 5: Copy QML plugin files ────────────────────────────────────────────
Write-Host "`n[5/6] Copying QML plugin..." -ForegroundColor Cyan

# Files that live in the imports output directory
$ImportsSrc = Join-Path $ImportsDir 'ConcertoBus'
foreach ($f in @('qmldir', 'ConcertoBusQml.qmltypes')) {
    $src = Join-Path $ImportsSrc $f
    if (Test-Path $src) {
        Copy-Item $src -Destination $ImportsOut
        Write-Host "      $f"
    } else {
        Write-Warning "  Not found (skipped): $src"
    }
}

# DLLs that CMake puts in the main build output directory
foreach ($dll in @('ConcertoBusQml.dll', 'concertobusplugin.dll')) {
    $src = Join-Path $BuildDir $dll
    if (Test-Path $src) {
        Copy-Item $src -Destination $ImportsOut
        Write-Host "      $dll"
    } else {
        Write-Warning "  Not found (skipped): $src"
    }
}

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host "`n── Deployment package ready ──────────────────────────────────────" -ForegroundColor Green
Write-Host "   Location : $OutDir"

$DaemonFiles  = (Get-ChildItem $DaemonOut  -File).Count
$PluginFiles  = (Get-ChildItem $ImportsOut -File).Count
Write-Host "   daemon\  : $DaemonFiles files"
Write-Host "   imports\ : $PluginFiles files"

Write-Host "`n   Usage:"
Write-Host "     Start the bus  : daemon\ConcertoBusDaemon.exe [--port 49152] [--catalog catalog.json]"
Write-Host "     QML client app : set QML_IMPORT_PATH=<package>\imports"
Write-Host "                      import ConcertoBus 1.0"
Write-Host ""
