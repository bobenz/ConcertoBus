<#
.SYNOPSIS
    Assembles a self-contained copy-pastable XMPP demo package for Windows.

.DESCRIPTION
    Produces dist\ConcertoBus-XmppDemo\ with two ready-to-run runner directories:

      runner-a\   — hosts SensorApp; connects as <UserA>@<XmppServer>
      runner-b\   — hosts DisplayApp; connects as <UserB>@<XmppServer>

    Each directory is fully self-contained (pm.exe, Qt/QXmpp DLLs, gateway plugin,
    client.exe, QML plugin, demo app QML).  Copy runner-a to Machine A and runner-b
    to Machine B, fill passwords in config.qml, double-click Run.bat.

.PARAMETER BuildDir
    RelWithDebInfo output directory of the main CMake build.
    Default: <repo>\build\RelWithDebInfo

.PARAMETER ImportsDir
    CMake QML imports output directory.
    Default: <repo>\build\imports

.PARAMETER OutDir
    Destination for the demo package.
    Default: <repo>\dist\ConcertoBus-XmppDemo

.PARAMETER QtBin
    Qt6 MSVC bin directory (windeployqt.exe + Qt DLLs).
    Default: D:\Qt\6.10.3\msvc2022_64\bin

.PARAMETER QxmppBin
    Directory containing QXmppQt6.dll.
    Default: D:\Qt\qxmpp-installed\bin

.PARAMETER MsvcBin
    MSVC toolchain bin\Hostx64\x64 directory. Auto-detected if left empty.

.EXAMPLE
    # Build first, then package:
    cmake -B build -DCONCERTO_BUS_XMPP=ON -DCMAKE_PREFIX_PATH="D:/Qt/6.10.3/msvc2022_64"
    cmake --build build --config RelWithDebInfo
    .\deploy\Deploy-XmppDemo.ps1
#>

[CmdletBinding()]
param(
    [string] $BuildDir   = (Join-Path $PSScriptRoot '..\build\RelWithDebInfo'),
    [string] $ImportsDir = (Join-Path $PSScriptRoot '..\build\imports'),
    [string] $OutDir     = (Join-Path $PSScriptRoot '..\dist\ConcertoBus-XmppDemo'),
    [string] $QtBin      = 'D:\Qt\6.10.3\msvc2022_64\bin',
    [string] $QxmppBin   = 'D:\Qt\qxmpp-installed\bin',
    [string] $MsvcBin    = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot   = Resolve-Path (Join-Path $PSScriptRoot '..')
$BuildDir   = Resolve-Path $BuildDir
$ImportsDir = Resolve-Path $ImportsDir

# ── helpers ───────────────────────────────────────────────────────────────────
function Require-File([string]$path, [string]$hint) {
    if (-not (Test-Path $path)) { Write-Error "$path not found. $hint" }
}

function Copy-WithLog([string]$src, [string]$dest) {
    if (Test-Path $src) {
        Copy-Item $src -Destination $dest -Force
        Write-Host "      $(Split-Path $src -Leaf)"
    } else {
        Write-Warning "  MISSING (skipped): $src"
    }
}

# ── windeployqt helper (runs against pm.exe or client.exe once per runner) ───
function Run-WinDeployQt([string]$exePath) {
    $wd = Join-Path $QtBin 'windeployqt.exe'
    Require-File $wd "-QtBin parameter"
    & $wd --no-quick-import --no-translations --no-system-d3d-compiler --release $exePath
    if ($LASTEXITCODE -ne 0) { Write-Error "windeployqt failed for $exePath" }
}

# ── MSVC runtime detection ────────────────────────────────────────────────────
if ($MsvcBin -eq '') {
    foreach ($ed in @('Enterprise','Professional','Community','BuildTools')) {
        $c = "C:\Program Files\Microsoft Visual Studio\2022\$ed\VC\Tools\MSVC"
        if (Test-Path $c) {
            $latest = Get-ChildItem $c -Directory | Sort-Object Name | Select-Object -Last 1
            $cand = Join-Path $latest.FullName 'bin\Hostx64\x64'
            if (Test-Path (Join-Path $cand 'vcruntime140.dll')) { $MsvcBin = $cand; break }
        }
    }
}
$MsvcDlls = @(
    'vcruntime140.dll','vcruntime140_1.dll',
    'msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll',
    'msvcp140_atomic_wait.dll','msvcp140_codecvt_ids.dll'
)

# ── Step 0: Validate required build outputs ───────────────────────────────────
Write-Host "`n[0/5] Validating build outputs..." -ForegroundColor Cyan
Require-File (Join-Path $BuildDir 'pm.exe')      "Build the project first."
Require-File (Join-Path $BuildDir 'client.exe')  "Build the project first."
Require-File (Join-Path $BuildDir 'gateways\XmppGateway.dll') "Build with -DCONCERTO_BUS_XMPP=ON."
Require-File (Join-Path $QxmppBin 'QXmppQt6.dll') "-QxmppBin parameter"
Write-Host "      OK"

# ── Step 1: Scaffold output directories ───────────────────────────────────────
Write-Host "`n[1/5] Creating output directories..." -ForegroundColor Cyan
foreach ($runner in @('runner-a','runner-b')) {
    foreach ($sub in @('','gateways','apps\SensorApp','apps\DisplayApp','imports\ConcertoBus')) {
        New-Item -ItemType Directory -Force -Path (Join-Path $OutDir "$runner\$sub") | Out-Null
    }
}
Write-Host "      $OutDir"

# ── Step 2: Copy binaries into each runner ────────────────────────────────────
Write-Host "`n[2/5] Copying binaries..." -ForegroundColor Cyan
foreach ($runner in @('runner-a','runner-b')) {
    $rd = Join-Path $OutDir $runner
    Write-Host "  [$runner]"

    # Daemon + client host
    Copy-WithLog (Join-Path $BuildDir 'pm.exe')     $rd
    Copy-WithLog (Join-Path $BuildDir 'client.exe') $rd

    # Gateway plugin
    Copy-WithLog (Join-Path $BuildDir 'gateways\XmppGateway.dll') (Join-Path $rd 'gateways')

    # QXmpp + Qt6Xml (not pulled by windeployqt)
    Copy-WithLog (Join-Path $QxmppBin 'QXmppQt6.dll') $rd
    Copy-WithLog (Join-Path $QtBin    'Qt6Xml.dll')    $rd

    # MSVC runtime
    if ($MsvcBin -ne '') {
        foreach ($dll in $MsvcDlls) { Copy-WithLog (Join-Path $MsvcBin $dll) $rd }
    }

    # QML plugin
    $importsSrc = Join-Path $ImportsDir 'ConcertoBus'
    $importsOut = Join-Path $rd 'imports\ConcertoBus'
    foreach ($f in @('qmldir','ConcertoBusQml.qmltypes')) {
        Copy-WithLog (Join-Path $importsSrc $f) $importsOut
    }
    foreach ($dll in @('ConcertoBusQml.dll','concertobusplugin.dll')) {
        Copy-WithLog (Join-Path $BuildDir $dll) $importsOut
    }
}

# ── Step 3: Run windeployqt (Qt DLLs — run against pm.exe once per runner) ───
Write-Host "`n[3/5] Running windeployqt..." -ForegroundColor Cyan
foreach ($runner in @('runner-a','runner-b')) {
    Write-Host "  [$runner]"
    $pmExe = Join-Path $OutDir "$runner\pm.exe"
    Run-WinDeployQt $pmExe
    # Also pull Qt DLLs needed by client.exe (QML/Quick)
    Run-WinDeployQt (Join-Path $OutDir "$runner\client.exe")
}

# ── Step 4: Copy demo app QML files ──────────────────────────────────────────
Write-Host "`n[4/5] Copying demo QML apps..." -ForegroundColor Cyan
$DemoDir = Join-Path $RepoRoot 'demo'

foreach ($app in @('SensorApp','DisplayApp')) {
    foreach ($runner in @('runner-a','runner-b')) {
        $dest = Join-Path $OutDir "$runner\apps\$app"
        foreach ($f in Get-ChildItem (Join-Path $DemoDir $app) -File) {
            Copy-WithLog $f.FullName $dest
        }
    }
}
Write-Host "      OK"

# ── Step 5: Write per-runner config.qml and Run.bat ──────────────────────────
Write-Host "`n[5/5] Writing config.qml and Run.bat files..." -ForegroundColor Cyan

# Runner A config
$configA = @'
// XMPP Demo — Runner A
// Edit 'password' before running.
// Run: double-click Run.bat  (or: pm.exe -c config.qml)

import QtQml 2.0
import ConcertoBusConfig 1.0

BusConfig {
    gateways: [
        Gateway {
            plugin:   "xmpp"
            user:     "client1@xmpp.credics"
            password: "CHANGE_ME"
            peers:    ["client2@xmpp.credics"]
        }
    ]

    App { id: sns; path: "apps/SensorApp" }
    Component.onCompleted: sns.start()
}
'@

# Runner B config
$configB = @'
// XMPP Demo — Runner B
// Edit 'password' before running.
// Run: double-click Run.bat  (or: pm.exe -c config.qml)

import QtQml 2.0
import ConcertoBusConfig 1.0

BusConfig {
    gateways: [
        Gateway {
            plugin:   "xmpp"
            user:     "client2@xmpp.credics"
            password: "CHANGE_ME"
            peers:    ["client1@xmpp.credics"]
        }
    ]

    App { id: dsp; path: "apps/DisplayApp" }
    Component.onCompleted: dsp.start()
}
'@

Set-Content -Path (Join-Path $OutDir 'runner-a\config.qml') -Value $configA -Encoding UTF8
Set-Content -Path (Join-Path $OutDir 'runner-b\config.qml') -Value $configB -Encoding UTF8
Write-Host "      config.qml (passwords left as CHANGE_ME)"

# Run.bat for each runner
$runBat = '@echo off
cd /d "%~dp0"
set QML_IMPORT_PATH=%~dp0imports
pm.exe -c config.qml
pause
'
Set-Content -Path (Join-Path $OutDir 'runner-a\Run.bat') -Value $runBat -Encoding ASCII
Set-Content -Path (Join-Path $OutDir 'runner-b\Run.bat') -Value $runBat -Encoding ASCII
Write-Host "      Run.bat"

# Top-level README
$readme = @"
ConcertoBus XMPP Federation Demo
=================================

Two machines (or two terminals on one machine) connected via XMPP.
SensorApp on Runner A publishes sensor readings that DisplayApp on Runner B receives
transparently through XMPP — no direct network connection between the machines needed.

Topology
--------
  Runner A (client1@xmpp.credics)           Runner B (client2@xmpp.credics)
    SensorApp publishes "sensor"  ─ XMPP ─►  DisplayApp subscribes "sensor"

Quick Start
-----------
1. Edit runner-a\config.qml  — set 'password' for client1@xmpp.credics
2. Edit runner-b\config.qml  — set 'password' for client2@xmpp.credics
3. Copy runner-a\ to Machine A  (or keep on this machine for a local test)
4. Copy runner-b\ to Machine B  (or open a second terminal)
5. On Machine A: double-click runner-a\Run.bat
6. On Machine B: double-click runner-b\Run.bat

Both runners connect to the XMPP server.  Within a few seconds you will see
sensor values appear in the Runner B window.

Requirements
------------
- Windows 10/11 x64
- No installation required — all DLLs are bundled
- Internet access to xmpp.credics:5222 (plain TCP, no TLS)

Troubleshooting
---------------
- "XmppGateway: error"     → check password in config.qml
- Values not arriving      → make sure both Run.bat windows are open
- Port 5222 blocked        → ask your network admin to allow outbound TCP 5222
"@

Set-Content -Path (Join-Path $OutDir 'README.txt') -Value $readme -Encoding UTF8
Write-Host "      README.txt"

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host "`n── Demo package ready ────────────────────────────────────────────" -ForegroundColor Green
Write-Host "   Location : $OutDir"
foreach ($runner in @('runner-a','runner-b')) {
    $n = (Get-ChildItem (Join-Path $OutDir $runner) -Recurse -File).Count
    Write-Host "   $runner\  : $n files"
}
Write-Host ""
Write-Host "   Next steps:"
Write-Host "   1. Edit runner-a\config.qml and runner-b\config.qml (set passwords)"
Write-Host "   2. Double-click Run.bat in each runner directory"
Write-Host ""
