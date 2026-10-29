@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo  Inline - One-Click Windows Installer
echo ==============================================================================
echo.

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not installed or not in PATH.
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

echo Configuring build for Windows...
cmake -B build_win -DBUILD_WINDOWS=ON -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

echo.
echo Building Inline...
cmake --build build_win --config Release -j%NUMBER_OF_PROCESSORS%
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

set EXE_PATH=build_win\gui\Inline.exe
if not exist "%EXE_PATH%" (
    set EXE_PATH=build_win\gui\Release\Inline.exe
)

if not exist "%EXE_PATH%" (
    echo [ERROR] Could not find compiled Inline.exe
    pause
    exit /b 1
)

echo.
echo ==============================================================================
echo [SUCCESS] Inline build completed successfully!
echo Executable location: %EXE_PATH%
echo ==============================================================================
echo.

set /p RUN_NOW="Would you like to launch Inline now? (Y/N): "
if /i "%RUN_NOW%"=="Y" (
    start "" "%EXE_PATH%"
    echo Inline launched in your system tray!
)

pause
