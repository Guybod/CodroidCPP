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

echo [1/4] Copy headers...
xcopy /y /i include\Codroid\* "%PKG_DIR%\include\Codroid\" >nul

echo [2/4] Copy binaries...
copy /y "%BUILD_DIR%\%CONFIG%\Codroid.dll" "%PKG_DIR%\bin\" >nul
copy /y "%BUILD_DIR%\%CONFIG%\Codroid.lib" "%PKG_DIR%\lib\" >nul

echo [3/4] Copy examples and docs...
copy /y examples_client\*.cpp "%PKG_DIR%\examples\" >nul
copy /y README.md "%PKG_DIR%\" >nul
copy /y SDK_GUIDE.md "%PKG_DIR%\docs\" >nul
if exist LICENSE copy /y LICENSE "%PKG_DIR%\" >nul

echo [4/4] Generate package guide...
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

echo ==================================================
echo Package created: %PKG_DIR%
echo ==================================================
pause
exit /b 0

:ERROR
echo.
echo !!!!!!!!!!!!!!! PACKAGE FAILED !!!!!!!!!!!!!!!
echo.
pause
exit /b 1
