<#
.SYNOPSIS
    Developer driver for System Stuff.

.EXAMPLE
    .\dd.ps1 run          # build Release x64 and launch
    .\dd.ps1 build        # build only
    .\dd.ps1 run -Configuration Debug
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'run')]
    [string]$Command = 'run',

    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64', 'Win32')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$project = Join-Path $root 'src\sys-stuff.vcxproj'

# TargetName in the .vcxproj: <project>-64, -32, with a 'd' suffix for Debug.
$suffix = if ($Platform -eq 'x64') { '64' } else { '32' }
if ($Configuration -eq 'Debug') { $suffix += 'd' }
$exe = Join-Path $root "exe\sys-stuff-$suffix.exe"

function Find-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Install Visual Studio 2022 17.14+ or Visual Studio 2026."
    }
    $found = & $vswhere -latest -prerelease -products * `
        -requires Microsoft.Component.MSBuild `
        -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (-not $found) { throw "MSBuild not found. Install the 'Desktop development with C++' workload." }
    return $found
}

function Invoke-Build {
    # The linker fails with access denied if a previous instance still holds the exe.
    Get-Process -Name "sys-stuff-$suffix" -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "Stopping running instance (PID $($_.Id))..." -ForegroundColor DarkGray
        $_ | Stop-Process -Force
        $_.WaitForExit(5000) | Out-Null
    }

    $msbuild = Find-MSBuild
    Write-Host "Building $Configuration|$Platform..." -ForegroundColor Cyan
    & $msbuild $project "/p:Configuration=$Configuration" "/p:Platform=$Platform" /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit code $LASTEXITCODE)." }
    Write-Host "Built $exe" -ForegroundColor Green
}

function Invoke-Run {
    if (-not (Test-Path $exe)) { throw "Executable not found: $exe" }
    Write-Host "Launching $(Split-Path $exe -Leaf)..." -ForegroundColor Cyan
    Start-Process -FilePath $exe | Out-Null
}

switch ($Command) {
    'build' { Invoke-Build }
    'run' { Invoke-Build; Invoke-Run }
}
