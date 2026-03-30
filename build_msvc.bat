@echo off
setlocal enabledelayedexpansion

:: 设置构建目录名称
set BUILD_DIR=build_msvc

echo ==================================================
echo      Codroid SDK MSVC Auto Builder (x64)
echo ==================================================
echo.
echo pPlease choice Visual Studio version:
echo [1] Visual Studio 2019 (v16)
echo [2] Visual Studio 2022 (v17)
echo [3] Visual Studio 2026 (v18)
echo.

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
    echo [Error] invalied input，please input 1, 2 或 3。
    goto CHOOSE_VS
)

:: 1. 清理旧的构建目录
if exist %BUILD_DIR% (
    echo [1/4] Cleaning old build directory...
    rd /s /q %BUILD_DIR%
)

:: 2. 创建并进入目录
echo [2/4] Creating build directory...
mkdir %BUILD_DIR%
cd %BUILD_DIR%

:: 3. 配置 CMake (生成解决方案)
echo [3/4] Configuring CMake for %VS_VERSION%...
:: 进入目录执行 cmake，注意 .. 指向源代码根目录
cmake -S .. -B %BUILD_DIR% -G "%VS_VERSION%" -A x64
if %errorlevel% neq 0 goto ERROR

:: 4. 执行编译 - DEBUG 版本
echo [4/4] Building DEBUG configuration...
cmake --build %BUILD_DIR% --config Debug
if %errorlevel% neq 0 goto ERROR

:: 5. 执行编译 - RELEASE 版本
echo Building RELEASE configuration...
cmake --build %BUILD_DIR% --config Release
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
