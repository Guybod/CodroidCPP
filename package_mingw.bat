@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set BUILD_DIR=build_mingw\release
set PKG_ROOT=package
set PKG_DIR=%PKG_ROOT%\CodroidSDK-Windows-MinGW-x64

echo ==================================================
echo   Packaging Codroid SDK for Windows MinGW x64
echo ==================================================

if not exist "%BUILD_DIR%\libCodroid.dll" (
    echo [Error] %BUILD_DIR%\libCodroid.dll not found.
    echo Run build_mingw.bat first and build Release.
    goto ERROR
)

if not exist "%BUILD_DIR%\libCodroid.dll.a" (
    echo [Error] %BUILD_DIR%\libCodroid.dll.a not found.
    echo Run build_mingw.bat first and build Release.
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

echo [1/5] Copy headers...
xcopy /y /i include\Codroid\* "%PKG_DIR%\include\Codroid\" >nul

echo [2/5] Copy binaries...
copy /y "%BUILD_DIR%\libCodroid.dll" "%PKG_DIR%\bin\Codroid.dll" >nul
copy /y "%BUILD_DIR%\libCodroid.dll.a" "%PKG_DIR%\lib\libCodroid.dll.a" >nul

echo [3/5] Copy examples and docs...
copy /y examples_client\*.cpp "%PKG_DIR%\examples\" >nul
copy /y README.md "%PKG_DIR%\" >nul
copy /y SDK_GUIDE.md "%PKG_DIR%\docs\" >nul
if exist LICENSE copy /y LICENSE "%PKG_DIR%\" >nul

echo [4/5] Generate package guide...
(
echo # Codroid SDK Windows MinGW x64 Package
echo.
echo ## Contents
echo.
echo - `include/`: public headers
echo - `bin/`: runtime DLL
echo - `lib/`: import library ^(GNU^)
echo - `examples/`: example source files
echo - `docs/SDK_GUIDE.md`: SDK guide
echo.
echo ## MinGW / GCC
echo.
echo Add include directories:
echo.
echo - `CodroidSDK-Windows-MinGW-x64\include`
echo.
echo Add library directory:
echo.
echo - `CodroidSDK-Windows-MinGW-x64\lib`
echo.
echo Link input:
echo.
echo - `-lCodroid`
echo.
echo Runtime:
echo.
echo - Copy `bin\Codroid.dll` beside your `.exe`, or add `bin\` to PATH.
) > "%PKG_DIR%\README_PACKAGE.md"

set ARCHIVE=%PKG_ROOT%\CodroidSDK-Windows-MinGW-x64.zip
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
