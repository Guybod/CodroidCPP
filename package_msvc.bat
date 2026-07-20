@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set BUILD_DIR=build_msvc
set CONFIG=Release
set PKG_ROOT=package
set PKG_DIR=%PKG_ROOT%\CodroidSDK-Windows-x64

echo ==================================================
echo      Packaging Codroid SDK for Windows x64
echo ==================================================

if not exist "%BUILD_DIR%\%CONFIG%\Codroid.dll" (
    echo [Error] %BUILD_DIR%\%CONFIG%\Codroid.dll not found.
    echo Run build_msvc.bat first.
    goto ERROR
)

if not exist "%BUILD_DIR%\%CONFIG%\Codroid.lib" (
    echo [Error] %BUILD_DIR%\%CONFIG%\Codroid.lib not found.
    echo Run build_msvc.bat first.
    goto ERROR
)

if exist "%PKG_DIR%" (
    rd /s /q "%PKG_DIR%"
)

mkdir "%PKG_DIR%"
mkdir "%PKG_DIR%\include"
mkdir "%PKG_DIR%\include\Codroid"
mkdir "%PKG_DIR%\bin"
mkdir "%PKG_DIR%\lib"
mkdir "%PKG_DIR%\examples"
mkdir "%PKG_DIR%\docs"

echo [1/5] Copy public headers (whitelist)...
copy /y include\Codroid\client.hpp "%PKG_DIR%\include\Codroid\" >nul
copy /y include\Codroid\types.hpp "%PKG_DIR%\include\Codroid\" >nul
copy /y include\Codroid\CodroidExport.h "%PKG_DIR%\include\Codroid\" >nul
copy /y include\Codroid\console_utf8.hpp "%PKG_DIR%\include\Codroid\" >nul
copy /y include\Codroid\cri_realtime_dispatcher.hpp "%PKG_DIR%\include\Codroid\" >nul
copy /y include\Codroid\trajectory_generator.hpp "%PKG_DIR%\include\Codroid\" >nul
copy /y include\Codroid\trajectory_types.hpp "%PKG_DIR%\include\Codroid\" >nul
mkdir "%PKG_DIR%\include\nlohmann" >nul 2>nul
xcopy /y /e /i third_party\nlohmann\* "%PKG_DIR%\include\nlohmann\" >nul

echo [2/5] Copy binaries...
copy /y "%BUILD_DIR%\%CONFIG%\Codroid.dll" "%PKG_DIR%\bin\" >nul
copy /y "%BUILD_DIR%\%CONFIG%\Codroid.lib" "%PKG_DIR%\lib\" >nul

echo [3/5] Copy examples and docs...
copy /y examples\*.cpp "%PKG_DIR%\examples\" >nul
copy /y README.md "%PKG_DIR%\" >nul
copy /y SDK_GUIDE.md "%PKG_DIR%\docs\" >nul
if exist LICENSE copy /y LICENSE "%PKG_DIR%\" >nul

echo [4/5] Generate package guide...
(
echo # Codroid SDK Windows x64 Package
echo.
echo ## Contents
echo.
echo - `include/`: public headers
echo - `bin/`: runtime DLL
echo - `lib/`: import library
echo - `examples/`: example source files
echo - `docs/SDK_GUIDE.md`: SDK guide
echo.
echo ## Visual Studio / MSVC
echo.
echo Add include directories:
echo.
echo - `CodroidSDK-Windows-x64\include`
echo.
echo Add library directory:
echo.
echo - `CodroidSDK-Windows-x64\lib`
echo.
echo Add linker input:
echo.
echo - `Codroid.lib`
echo.
echo Runtime:
echo.
echo - Copy `bin\Codroid.dll` beside the application `.exe`, or add `bin\` to PATH.
) > "%PKG_DIR%\README_PACKAGE.md"

set ARCHIVE=%PKG_ROOT%\CodroidSDK-Windows-MSVC-x64.zip
echo [5/5] Create archive...
if exist "%ARCHIVE%" del /f /q "%ARCHIVE%"
powershell -NoProfile -Command "Compress-Archive -LiteralPath '%CD%\%PKG_DIR%' -DestinationPath '%CD%\%ARCHIVE%' -Force"
if errorlevel 1 (
    echo [Error] Failed to create %ARCHIVE%
    goto ERROR
)

echo ==================================================
echo Package created:
echo   dir : %PKG_DIR%
echo   zip : %ARCHIVE%
echo ==================================================
pause
exit /b 0

:ERROR
echo.
echo !!!!!!!!!!!!!!! PACKAGE FAILED !!!!!!!!!!!!!!!
echo.
pause
exit /b 1
