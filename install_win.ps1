# ==============================================================================
# Inline - One-Click Windows PowerShell Installer
# ==============================================================================

Write-Host "=== Installing Inline for Windows ===" -ForegroundColor Cyan
Write-Host ""

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake is not installed or not in your PATH. Please install from https://cmake.org/download/"
    exit 1
}

Write-Host "Configuring CMake build..." -ForegroundColor Yellow
cmake -B build_win -DBUILD_WINDOWS=ON -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed."
    exit 1
}

Write-Host "Compiling Inline..." -ForegroundColor Yellow
cmake --build build_win --config Release -j $env:NUMBER_OF_PROCESSORS
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed."
    exit 1
}

$exePath = "build_win\gui\Inline.exe"
if (-not (Test-Path $exePath)) {
    $exePath = "build_win\gui\Release\Inline.exe"
}

if (-not (Test-Path $exePath)) {
    Write-Error "Could not locate compiled Inline.exe"
    exit 1
}

Write-Host ""
Write-Host "✔ Installation successful!" -ForegroundColor Green
Write-Host "Executable located at: $exePath" -ForegroundColor White
Write-Host ""

$response = Read-Host "Would you like to launch Inline now? (Y/N)"
if ($response -eq 'Y' -or $response -eq 'y') {
    Start-Process $exePath
    Write-Host "Inline launched in your system tray!" -ForegroundColor Green
}
