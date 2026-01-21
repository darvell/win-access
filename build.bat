@echo off
setlocal enabledelayedexpansion

:: Clarity Layer Build Script
:: Usage: build.bat [preset] [target]
:: Examples:
::   build.bat                    - Build with msvc-release preset
::   build.bat clang-release      - Build with clang-release preset
::   build.bat msvc-debug         - Build with msvc-debug preset

set PRESET=%1
if "%PRESET%"=="" set PRESET=msvc-release

set TARGET=%2

echo.
echo ========================================
echo Clarity Layer Build Script
echo ========================================
echo.
echo Preset: %PRESET%
echo.

:: Check for VCPKG_ROOT
if "%VCPKG_ROOT%"=="" (
    echo ERROR: VCPKG_ROOT environment variable not set
    echo Please set VCPKG_ROOT to your vcpkg installation directory
    echo Example: set VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

:: Check for vcpkg
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo ERROR: vcpkg not found at %VCPKG_ROOT%
    exit /b 1
)

:: Check for required packages
echo Checking vcpkg packages...
"%VCPKG_ROOT%\vcpkg" list | findstr /i "nlohmann-json" > nul
if errorlevel 1 (
    echo Installing nlohmann-json...
    "%VCPKG_ROOT%\vcpkg" install nlohmann-json:x64-windows
)

"%VCPKG_ROOT%\vcpkg" list | findstr /i "spdlog" > nul
if errorlevel 1 (
    echo Installing spdlog...
    "%VCPKG_ROOT%\vcpkg" install spdlog:x64-windows
)

:: Configure
echo.
echo Configuring with preset: %PRESET%
cmake --preset %PRESET%
if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed
    exit /b 1
)

:: Build
echo.
echo Building...
if "%TARGET%"=="" (
    cmake --build --preset %PRESET%
) else (
    cmake --build --preset %PRESET% --target %TARGET%
)

if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo ========================================
echo Build complete!
echo ========================================
echo.
echo Output: build\%PRESET%\ClarityLayer.exe
echo.

endlocal
