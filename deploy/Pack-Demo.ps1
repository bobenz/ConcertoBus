<#
.SYNOPSIS
    Builds a portable ConcertoBus demo package.

.DESCRIPTION
    Assembles a self-contained directory containing:
      x64\          pm.exe (Qt6 daemon) + client.exe (Qt6 QML host) + Qt6 DLLs
      x86\          Qt5ClientApp.exe + Qt5 DLLs
      demo\         config files and QML app sources
      Run-Demo1.bat launch the stdio auto-launch demo
      Run-Demo2.bat launch the TCP on-demand demo
      README.txt

.PARAMETER BuildDir
    CMake RelWithDebInfo output directory (contains pm.exe, client.exe).
    Default: <repo>\build\RelWithDebInfo

.PARAMETER Qt5BuildDir
    qmake output directory for the Qt5 client.
    Default: <repo>\build-qt5

.PARAMETER OutDir
    Destination directory for the package.
    Default: <repo>\dist\ConcertoBus-Demo

.PARAMETER Qt6Bin
    Qt 6 MSVC x64 bin directory (windeployqt6.exe + Qt6*.dll).
    Default: D:\Qt\6.10.3\msvc2022_64\bin

.PARAMETER Qt5Bin
    Qt 5 MSVC x86 bin directory (windeployqt.exe + Qt5*.dll).
    Default: D:\5.15.2\msvc2019\bin

.EXAMPLE
    .\deploy\Pack-Demo.ps1
    .\deploy\Pack-Demo.ps1 -OutDir C:\Packages\ConcertoBus-Demo
#>

[CmdletBinding()]
param(
    [string] $BuildDir    = (Join-Path $PSScriptRoot '..\build\RelWithDebInfo'),
    [string] $Qt5BuildDir = (Join-Path $PSScriptRoot '..\build-qt5'),
    [string] $OutDir      = (Join-Path $PSScriptRoot '..\dist\ConcertoBus-Demo'),
    [string] $Qt6Bin      = 'D:\Qt\6.10.3\msvc2022_64\bin',
    [string] $Qt5Bin      = 'D:\5.15.2\msvc2019\bin'
)

Set-StrictMode  -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$BuildDir = Resolve-Path $BuildDir

# -- helpers ------------------------------------------------------------------
function Step([string]$msg) { Write-Host "`n$msg" -ForegroundColor Cyan }
function Ok  ([string]$msg) { Write-Host "  $msg" -ForegroundColor Green }
function Warn([string]$msg) { Write-Warning $msg }

function Require([string]$path, [string]$label) {
    if (-not (Test-Path $path)) {
        throw "Required $label not found: $path"
    }
}

# -- clean output -------------------------------------------------------------
if (Test-Path $OutDir) {
    Remove-Item $OutDir -Recurse -Force
}
$x64Dir  = New-Item -ItemType Directory (Join-Path $OutDir 'x64')  | Select-Object -Exp FullName
$x86Dir  = New-Item -ItemType Directory (Join-Path $OutDir 'x86')  | Select-Object -Exp FullName
$demoDir = New-Item -ItemType Directory (Join-Path $OutDir 'demo') | Select-Object -Exp FullName
Write-Host "Output: $OutDir" -ForegroundColor Yellow

# -- Step 1: Qt6 binaries (x64) -----------------------------------------------
Step '[1/5] Qt6 x64 -- pm.exe + client.exe'

$wdq6 = Join-Path $Qt6Bin 'windeployqt6.exe'
if (-not (Test-Path $wdq6)) { $wdq6 = Join-Path $Qt6Bin 'windeployqt.exe' }
Require $wdq6 'windeployqt (Qt6)'

foreach ($exe in @('pm.exe', 'client.exe')) {
    $src = Join-Path $BuildDir $exe
    Require $src $exe
    Copy-Item $src $x64Dir
    Ok $exe

    & $wdq6 --dir $x64Dir --no-translations --no-system-d3d-compiler `
             --no-quick-import --no-opengl-sw $src | Out-Null
}
Ok "windeployqt6 done ($(@(Get-ChildItem $x64Dir -File).Count) files)"

# -- Step 2: Qt5 binary (x86) -------------------------------------------------
Step '[2/5] Qt5 x86 -- Qt5ClientApp.exe'

$wdq5 = Join-Path $Qt5Bin 'windeployqt.exe'
Require $wdq5 'windeployqt (Qt5)'

$qt5Exe = Join-Path $Qt5BuildDir 'Qt5ClientApp.exe'
Require $qt5Exe 'Qt5ClientApp.exe'
Copy-Item $qt5Exe $x86Dir
Ok 'Qt5ClientApp.exe'

$savedPath = $env:PATH
$env:PATH = "$Qt5Bin;$env:PATH"
& $wdq5 --dir $x86Dir --no-translations --no-system-d3d-compiler `
         --no-quick-import --no-opengl-sw $qt5Exe | Out-Null
$env:PATH = $savedPath
Ok "windeployqt5 done ($(@(Get-ChildItem $x86Dir -File).Count) files)"

# -- Step 3: demo files -------------------------------------------------------
Step '[3/5] Demo files'

$demoDirs = @('SensorApp', 'DisplayApp', 'ControllerApp', 'tcp-demo')
foreach ($d in $demoDirs) {
    $src = Join-Path $RepoRoot "demo\$d"
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $demoDir $d) -Recurse
        Ok $d
    } else {
        Warn "demo\$d not found -- skipped"
    }
}
Copy-Item (Join-Path $RepoRoot 'demo\config.qml') $demoDir
Ok 'config.qml'

# -- Step 4: launcher batch files ---------------------------------------------
Step '[4/5] Launcher scripts'

@'
@echo off
REM Demo 1 -- stdio auto-launch.
REM pm starts Sensor and Display automatically; both register via stdio pipes.
REM Qt5Listener (x86) connects via TCP and receives "sensor" messages.

cd /d "%~dp0"
start "pm (Qt6 daemon)" x64\pm.exe -c demo\config.qml
timeout /t 2 /nobreak >nul
start "Qt5Listener (Qt5)" x86\Qt5ClientApp.exe
echo.
echo Sensor and Display are running inside pm.
echo Qt5ClientApp.exe is listening for "sensor" messages via TCP.
echo Close this window or press Ctrl+C to exit all processes.
pause
taskkill /F /IM pm.exe /IM Qt5ClientApp.exe 2>nul
'@ | Set-Content (Join-Path $OutDir 'Run-Demo1.bat') -Encoding ASCII
Ok 'Run-Demo1.bat'

@'
@echo off
REM Demo 2 -- TCP on-demand launch.
REM pm knows about Display but doesn't start it.
REM Controller (Qt6 client) connects, requests launch, then publishes sensor data.
REM Qt5Listener also connects and receives the same data.

cd /d "%~dp0"
start "pm (Qt6 daemon)" x64\pm.exe -c demo\tcp-demo\config.qml
timeout /t 2 /nobreak >nul
start "Controller (Qt6)" x64\client.exe demo\ControllerApp\Launch.qml
timeout /t 1 /nobreak >nul
start "Qt5Listener (Qt5)" x86\Qt5ClientApp.exe
echo.
echo Controller will launch Display on demand and publish sensor data.
echo Qt5ClientApp.exe is listening for "sensor" messages via TCP.
echo Close this window or press Ctrl+C to exit all processes.
pause
taskkill /F /IM pm.exe /IM client.exe /IM Qt5ClientApp.exe 2>nul
'@ | Set-Content (Join-Path $OutDir 'Run-Demo2.bat') -Encoding ASCII
Ok 'Run-Demo2.bat'

# -- Step 5: README -----------------------------------------------------------
Step '[5/5] README'

@'
ConcertoBus Demo Package
========================

Contents
--------
  x64\              Qt6 x64 binaries
    pm.exe          Bus daemon (process manager)
    client.exe      Generic QML application host
    Qt6*.dll        Qt 6 runtime DLLs

  x86\              Qt5 x86 binaries
    Qt5ClientApp.exe  Minimal TCP listener demo (built with Qt 5.15)
    Qt5*.dll          Qt 5 runtime DLLs

  demo\             QML application sources
    config.qml        Demo 1 config  (auto-launch both apps)
    tcp-demo\         Demo 2 config  (on-demand launch via TCP)
    SensorApp\        Publishes a counter to the "sensor" topic
    DisplayApp\       Subscribes to "sensor" and prints readings
    ControllerApp\    TCP client that launches Display on demand

Quick start
-----------
  Double-click Run-Demo1.bat  --  auto-launch demo (Sensor + Display + Qt5 listener)
  Double-click Run-Demo2.bat  --  TCP on-demand demo (Controller + Display + Qt5 listener)

Manual usage
------------
  # Start the daemon with a config file
  x64\pm.exe -c demo\config.qml

  # Run any QML app through the generic host
  x64\client.exe demo\SensorApp\Launch.qml

  # Connect a Qt5 listener (receives "sensor" topic)
  x86\Qt5ClientApp.exe

Wire protocol (TCP port 49152, newline-delimited JSON)
------------------------------------------------------
  Send:    {"cmd":"register","name":"MyApp"}
  Receive: {"ok":true}

  Send:    {"cmd":"subscribe","tag":"sensor"}
  Send:    {"cmd":"publish","to":"sensor","data":{"value":42}}
  Receive: {"push":true,"from":"sensor","sender":"Sensor","data":{"value":42}}

See docs\ConcertoBus-Guide.md in the source repository for the full protocol
reference and how to build your own apps on top of the bus.

System requirements
-------------------
  Demo 1 / 2  : Windows 10+, no install required (DLLs included)
  Qt5 client  : 32-bit (x86) process -- runs on any 64-bit Windows
  Qt6 daemon  : 64-bit (x64) process

'@ | Set-Content (Join-Path $OutDir 'README.txt') -Encoding UTF8
Ok 'README.txt'

# -- summary ------------------------------------------------------------------
$total = @(Get-ChildItem $OutDir -Recurse -File).Count
Write-Host "`n-- Package ready --" -ForegroundColor Green
Write-Host "   Location : $OutDir"
Write-Host "   x64\     : $(@(Get-ChildItem $x64Dir  -File).Count) files"
Write-Host "   x86\     : $(@(Get-ChildItem $x86Dir  -File).Count) files"
Write-Host "   demo\    : $(@(Get-ChildItem $demoDir -Recurse -File).Count) files"
Write-Host "   Total    : $total files"
Write-Host ""
