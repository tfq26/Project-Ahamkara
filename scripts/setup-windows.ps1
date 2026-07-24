# setup-windows.ps1
#
# Windows development environment setup for Ahamkara.
# Installs dependencies and configures a CMake preset.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/setup-windows.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/setup-windows.ps1 -Preset debug-headless
#   powershell -ExecutionPolicy Bypass -File scripts/setup-windows.ps1 -SkipConfigure
#
# Options:
#   -Preset <name>     CMake preset to configure (default: debug)
#   -SkipConfigure     Only verify dependencies, skip cmake --preset
#   -Help              Show this help

param(
    [string]$Preset = "debug",
    [switch]$SkipConfigure,
    [switch]$Help
)

if ($Help) {
    Write-Host @"
Usage:
  .\scripts\setup-windows.ps1 [-Preset <name>] [-SkipConfigure]

Options:
  -Preset <name>     CMake preset to configure (default: debug)
  -SkipConfigure     Only verify dependencies and environment
  -Help              Show this help
"@
    exit 0
}

function Test-Command($cmd) {
    $found = Get-Command $cmd -ErrorAction SilentlyContinue
    return ($null -ne $found)
}

$projectDir = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
Set-Location $projectDir

Write-Host "=== Ahamkara Windows Setup ===" -ForegroundColor Cyan
Write-Host "Project directory: $projectDir"
Write-Host ""

# --- Verify CMake ---
if (-not (Test-Command cmake)) {
    Write-Host "ERROR: cmake not found. Install CMake 3.20+ and ensure it is on PATH." -ForegroundColor Red
    exit 1
}
Write-Host "[OK] cmake $((cmake --version | Select-String -Pattern '\d+\.\d+\.\d+').Matches.Value)"

# --- Verify Ninja ---
if (-not (Test-Command ninja)) {
    Write-Host "WARNING: ninja not found. Install it via 'winget install Ninja-build.Ninja' or 'choco install ninja'." -ForegroundColor Yellow
    Write-Host "Falling back to MSBuild (slower)." -ForegroundColor Yellow
} else {
    Write-Host "[OK] ninja $((ninja --version 2>$null))"
}

# --- Verify MSVC ---
if (-not (Test-Command cl)) {
    Write-Host "ERROR: MSVC compiler (cl.exe) not found on PATH." -ForegroundColor Red
    Write-Host "Run this script from a Visual Studio Developer Command Prompt, or install" -ForegroundColor Red
    Write-Host "the 'Desktop development with C++' workload via Visual Studio Installer." -ForegroundColor Red
    exit 1
}
$clVer = &{cl.exe 2>&1 | Select-String -Pattern "^Microsoft.*" | Select-Object -First 1}
if ($clVer) {
    Write-Host "[OK] $($clVer.ToString().Trim())"
} else {
    Write-Host "[OK] MSVC compiler detected"
}

# --- Verify vcpkg for GLFW3 (only for full client builds) ---
if ($Preset -ne "debug-headless") {
    $vcpkgRoot = $env:VCPKG_ROOT
    if (-not $vcpkgRoot) {
        $vcpkgRoot = "$env:LOCALAPPDATA\vcpkg"
    }
    $vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        Write-Host "WARNING: vcpkg not found at '$vcpkgRoot'." -ForegroundColor Yellow
        Write-Host "Install vcpkg or set VCPKG_ROOT environment variable." -ForegroundColor Yellow
        Write-Host "Skipping GLFW3 dependency check." -ForegroundColor Yellow
    } else {
        Write-Host "[OK] vcpkg found at $vcpkgRoot"
        
        # Check if glfw3 is installed
        $glfwCheck = &$vcpkgExe list glfw3 2>$null
        if (-not $glfwCheck) {
            Write-Host "Installing glfw3 via vcpkg (x64-windows)..." -ForegroundColor Yellow
            &$vcpkgExe install glfw3 --triplet x64-windows
            if ($LASTEXITCODE -ne 0) {
                Write-Host "WARNING: Failed to install glfw3. Client builds may fail." -ForegroundColor Yellow
            } else {
                Write-Host "[OK] glfw3 installed"
            }
        } else {
            Write-Host "[OK] glfw3 already installed"
        }
    }
}

# --- Check for optional tools ---
if (Test-Command git) {
    Write-Host "[OK] git found"
} else {
    Write-Host "WARNING: git not found on PATH" -ForegroundColor Yellow
}

Write-Host ""

# --- Configure ---
if ($SkipConfigure) {
    Write-Host "Skipped CMake configure."
    exit 0
}

$toolchain = ""
if ($Preset -ne "debug-headless" -and $env:VCPKG_ROOT) {
    $toolchainPath = "$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    if (Test-Path $toolchainPath) {
        $toolchain = "-DCMAKE_TOOLCHAIN_FILE=`"$toolchainPath`""
    }
}

Write-Host "Configuring preset: $Preset" -ForegroundColor Cyan
$configCmd = "cmake --preset $Preset -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl"
if ($toolchain) {
    $configCmd += " $toolchain -DVCPKG_TARGET_TRIPLET=x64-windows"
}
Write-Host "Running: $configCmd" -ForegroundColor Gray
Invoke-Expression $configCmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "Setup complete for preset: $Preset" -ForegroundColor Green
} else {
    Write-Host "Setup FAILED for preset: $Preset (exit code: $LASTEXITCODE)" -ForegroundColor Red
    exit $LASTEXITCODE
}
