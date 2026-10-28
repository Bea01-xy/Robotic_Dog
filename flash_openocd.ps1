#Requires -Version 5.1
<#
.SYNOPSIS
    Build and flash firmware to STM32H723 via OpenOCD + ST-Link
.DESCRIPTION
    Windows equivalent of the original flash_openocd.sh.
    Auto-detects the configured CMake build directory (CMakePresets' build\<Preset>,
    legacy cmake-build-debug, or out\build), configures it if absent, then builds
    (optional) and flashes .elf.
.PARAMETER ElfPath
    Path to .elf file. Omit to build and use the configured build directory.
.PARAMETER BuildDir
    Explicit CMake build directory. Omit to auto-detect.
.PARAMETER NoBuild
    Skip build step, only flash existing .elf.
.EXAMPLE
    .\flash_openocd.ps1
.EXAMPLE
    .\flash_openocd.ps1 -BuildDir .\build\Debug -NoBuild
.EXAMPLE
    .\flash_openocd.ps1 -ElfPath .\build\Debug\Engineer_2026_H7_Template_new.elf -NoBuild
#>
param(
    [string]$ElfPath,
    [string]$BuildDir,
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$CfgPath = Join-Path $ProjectRoot "stm32h723.cfg"
$ElfName = "Engineer_2026_H7_Template_new.elf"
$DefaultBuildDir = Join-Path $ProjectRoot "build\Debug"

function Resolve-BuildDir([string]$Requested) {
    if ($Requested) {
        $resolved = Resolve-Path $Requested -ErrorAction SilentlyContinue
        if ($resolved) { return $resolved.Path }
        return $Requested
    }

    # Preset output dirs first (CMakePresets.json), then CLion legacy dir.
    $candidates = @(
        (Join-Path $ProjectRoot "build\Debug"),
        (Join-Path $ProjectRoot "build\RelWithDebInfo"),
        (Join-Path $ProjectRoot "build\Release"),
        (Join-Path $ProjectRoot "build\MinSizeRel"),
        (Join-Path $ProjectRoot "cmake-build-debug")
    )
    foreach ($parent in @("build", "out\build")) {
        Get-ChildItem (Join-Path $ProjectRoot $parent) -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { $candidates += $_.FullName }
    }

    foreach ($dir in ($candidates | Select-Object -Unique)) {
        if (Test-Path (Join-Path $dir "CMakeCache.txt") -PathType Leaf) { return $dir }
    }
    return $DefaultBuildDir
}

$BuildDir = Resolve-BuildDir $BuildDir

# --------------- Tool detection ---------------

function Find-Cmake {
    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $cachePath -PathType Leaf) {
        $cacheEntry = Get-Content $cachePath | Where-Object { $_ -like "CMAKE_COMMAND:INTERNAL=*" } | Select-Object -First 1
        if ($cacheEntry) {
            $cachedCmake = ($cacheEntry -split "=", 2)[1]
            if (Test-Path $cachedCmake -PathType Leaf) { return $cachedCmake }
        }
    }

    $cmake = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
    if ($cmake) { return $cmake.Source }

    $paths = @(
        "$env:ProgramFiles\CMake\bin\cmake.exe",
        "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe",
        "$env:LOCALAPPDATA\Programs\CMake\bin\cmake.exe",
        "$env:USERPROFILE\scoop\apps\cmake\current\bin\cmake.exe",
        "${env:ChocolateyInstall}\bin\cmake.exe"
    )
    $clionPaths = @(
        "$env:LOCALAPPDATA\JetBrains\CLion*\bin\cmake\win\*\bin\cmake.exe",
        "$env:ProgramFiles\JetBrains\CLion*\bin\cmake\win\*\bin\cmake.exe"
    )
    foreach ($pattern in $clionPaths) {
        $matches = Resolve-Path $pattern -ErrorAction SilentlyContinue
        if ($matches) { return $matches[-1].Path }
    }
    foreach ($p in $paths) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

function Find-OpenOcd {
    function Resolve-ScriptsDir([string]$ExePath) {
        $binDir = Split-Path $ExePath -Parent
        $candidates = @(
            (Join-Path $binDir "..\share\openocd\scripts"),
            (Join-Path $binDir "..\openocd\scripts"),
            (Join-Path $binDir "..\scripts"),
            (Join-Path $binDir "..\..\share\openocd\scripts"),
            (Join-Path (Join-Path $binDir "..") "openocd\scripts")
        )
        foreach ($c in $candidates) {
            $normalized = (Resolve-Path $c -ErrorAction SilentlyContinue)
            if ($normalized) {
                $testPath = Join-Path $normalized.Path "interface\stlink.cfg"
                if (Test-Path $testPath) { return $normalized.Path }
            }
        }
        return $null
    }

    $ocd = Get-Command "openocd.exe" -ErrorAction SilentlyContinue
    if ($ocd) {
        $scripts = Resolve-ScriptsDir $ocd.Source
        return @{ Exe = $ocd.Source; Scripts = $scripts }
    }

    $paths = @(
        "$env:ProgramFiles\OpenOCD\bin\openocd.exe",
        "${env:ProgramFiles(x86)}\OpenOCD\bin\openocd.exe",
        "$env:USERPROFILE\scoop\apps\openocd\current\bin\openocd.exe",
        "${env:ChocolateyInstall}\bin\openocd.exe"
    )
    foreach ($p in $paths) {
        if (Test-Path $p) {
            $scripts = Resolve-ScriptsDir $p
            return @{ Exe = $p; Scripts = $scripts }
        }
    }

    # xPack OpenOCD (both standard and custom install locations)
    $xpackPaths = @(
        "$env:LOCALAPPDATA\xpack\@xpack-dev-tools\openocd\*\.content\bin\openocd.exe",
        "$env:SYSTEMDRIVE\Development\xpack-openocd-*\bin\openocd.exe"
    )
    foreach ($pattern in $xpackPaths) {
        $matches = Resolve-Path $pattern -ErrorAction SilentlyContinue
        if ($matches) {
            $exe = $matches[-1].Path
            $scripts = Resolve-ScriptsDir $exe
            return @{ Exe = $exe; Scripts = $scripts }
        }
    }

    $msysPaths = @(
        "$env:SYSTEMDRIVE\msys64\mingw64\bin\openocd.exe",
        "$env:SYSTEMDRIVE\msys64\mingw32\bin\openocd.exe"
    )
    foreach ($p in $msysPaths) {
        if (Test-Path $p) {
            $scripts = Resolve-ScriptsDir $p
            return @{ Exe = $p; Scripts = $scripts }
        }
    }

    return $null
}

# --------------- Main ---------------

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  STM32H723 Build + Flash (Windows)"      -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# ---- 1. Check OpenOCD config ----
if (-not (Test-Path $CfgPath -PathType Leaf)) {
    Write-Host "[ERROR] OpenOCD config not found: $CfgPath" -ForegroundColor Red
    exit 1
}

# ---- 2. Resolve ELF ----
if ($ElfPath) {
    if (-not (Test-Path $ElfPath -PathType Leaf)) {
        Write-Host "[ERROR] ELF not found: $ElfPath" -ForegroundColor Red
        exit 1
    }
} else {
    if (-not $NoBuild) {
        $cmake = Find-Cmake
        if (-not $cmake) {
            Write-Host "[ERROR] cmake.exe not found; refusing to flash an existing ELF." -ForegroundColor Red
            exit 1
        }
        if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt") -PathType Leaf)) {
            Write-Host "Build directory not configured; configuring: $BuildDir" -ForegroundColor Yellow
            & $cmake -S $ProjectRoot -B $BuildDir -G Ninja
            if ($LASTEXITCODE -ne 0) {
                Write-Host "[ERROR] CMake configure failed for: $BuildDir" -ForegroundColor Red
                exit 1
            }
        }

        Write-Host "Building: $BuildDir" -ForegroundColor Green
        & $cmake --build $BuildDir
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[ERROR] Build failed" -ForegroundColor Red
            exit 1
        }
    }

    $ElfPath = Join-Path $BuildDir $ElfName
    if (-not (Test-Path $ElfPath -PathType Leaf)) {
        Write-Host "[ERROR] ELF not found in build directory: $ElfPath" -ForegroundColor Red
        Write-Host "  Run without -NoBuild, or pass -BuildDir <configured dir>." -ForegroundColor Yellow
        exit 1
    }
}

# Convert Windows backslashes to forward slashes for OpenOCD's TCL parser
$ElfPath = $ElfPath -replace "\\", "/"
$CfgPath = $CfgPath -replace "\\", "/"

Write-Host "ELF: $ElfPath" -ForegroundColor Green

# ---- 3. Find OpenOCD ----
$ocd = Find-OpenOcd
if (-not $ocd) {
    Write-Host "[ERROR] openocd.exe not found." -ForegroundColor Red
    Write-Host "  Install via: scoop install openocd" -ForegroundColor Yellow
    exit 1
}

$openocdExe = $ocd.Exe
$scriptsDir = $ocd.Scripts
if ($scriptsDir) { $scriptsDir = $scriptsDir -replace "\\", "/" }

Write-Host "OpenOCD: $openocdExe" -ForegroundColor Green
if ($scriptsDir) {
    Write-Host "Scripts: $scriptsDir" -ForegroundColor Green
} else {
    Write-Host "[WARN] OpenOCD scripts dir not found, proceeding without -s flag." -ForegroundColor Yellow
}

# ---- 4. Flash ----
Write-Host "Flashing..." -ForegroundColor Magenta

$ocdArgs = @()
if ($scriptsDir) {
    $ocdArgs += "-s"; $ocdArgs += $scriptsDir
}
$ocdArgs += "-f"; $ocdArgs += $CfgPath
$ocdArgs += "-c"; $ocdArgs += "tcl_port disabled"
$ocdArgs += "-c"; $ocdArgs += "gdb_port disabled"
$ocdArgs += "-c"; $ocdArgs += "program {$ElfPath} verify reset exit"

& $openocdExe $ocdArgs
$exit = $LASTEXITCODE

if ($exit -eq 0) {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  FLASH SUCCESSFUL!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Cyan
} else {
    Write-Host "[ERROR] OpenOCD exited with code: $exit" -ForegroundColor Red
}
exit $exit
