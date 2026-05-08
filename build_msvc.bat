@echo off
setlocal enabledelayedexpansion

:: Always run from script directory
cd /d "%~dp0"

:: Build output directory
set BUILD_DIR=build_msvc

echo ==================================================
echo      Codroid SDK MSVC Auto Builder (x64)
echo ==================================================
echo.
echo Please choose Visual Studio version:
echo [1] Visual Studio 2019 (v16)
echo [2] Visual Studio 2022 (v17)
echo [3] Visual Studio 2026 (v18)
echo.

set VS_VERSION=

:CHOOSE_VS
set /p VS_CHOICE="please input (1, 2 or 3): "

if "%VS_CHOICE%"=="1" (
    set VS_VERSION=Visual Studio 16 2019
    echo Visual Studio 2019
) else if "%VS_CHOICE%"=="2" (
    set VS_VERSION=Visual Studio 17 2022
    echo Visual Studio 2022
) else if "%VS_CHOICE%"=="3" (
    set VS_VERSION=Visual Studio 18 2026
    echo Visual Studio 2026
) else (
    echo [Error] invalid input, please input 1, 2 or 3.
    goto CHOOSE_VS
)

:: 1. Clean old build directory
if exist "%BUILD_DIR%" (
    echo [1/4] Cleaning old build directory...
    rd /s /q "%BUILD_DIR%"
)

:: 2. Create build directory
echo [2/4] Creating build directory...
mkdir "%BUILD_DIR%"

:: 3. Configure CMake (generate solution)
echo [3/4] Configuring CMake for %VS_VERSION%...
cmake -S . -B "%BUILD_DIR%" -G "%VS_VERSION%" -A x64
if %errorlevel% neq 0 goto ERROR

:: 4. Build Debug
echo [4/4] Building DEBUG configuration...
cmake --build "%BUILD_DIR%" --config Debug
if %errorlevel% neq 0 goto ERROR

:: 5. Build Release
echo Building RELEASE configuration...
cmake --build "%BUILD_DIR%" --config Release
if %errorlevel% neq 0 goto ERROR

echo.
echo ==================================================
echo                BUILD SUCCESSFUL ^!
echo ==================================================
echo.
echo DEBUG FILES:
echo   DLL: %BUILD_DIR%\Debug\Codroid.dll
echo   LIB: %BUILD_DIR%\Debug\Codroid.lib
echo.
echo RELEASE FILES:
echo   DLL: %BUILD_DIR%\Release\Codroid.dll
echo   LIB: %BUILD_DIR%\Release\Codroid.lib
echo.
echo ==================================================
pause
exit /b 0

:ERROR
echo.
echo !!!!!!!!!!!!!!! BUILD FAILED !!!!!!!!!!!!!!!
echo.
pause
exit /b %errorlevel%
