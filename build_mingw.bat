@echo off
setlocal enabledelayedexpansion

:: Always run from script directory
cd /d "%~dp0"

:: Build output directory
set BUILD_DIR=build_mingw

echo ==================================================
echo     Codroid SDK MinGW Auto Builder (x64)
echo ==================================================
echo.
echo Please choose build type:
echo [1] Debug
echo [2] Release
echo [3] Both (Debug + Release)
echo.

set BUILD_TYPE=

:CHOOSE_TYPE
set /p TYPE_CHOICE="please input (1, 2 or 3): "

if "%TYPE_CHOICE%"=="1" (
    set BUILD_TYPE=Debug
    echo Build type: Debug
) else if "%TYPE_CHOICE%"=="2" (
    set BUILD_TYPE=Release
    echo Build type: Release
) else if "%TYPE_CHOICE%"=="3" (
    set BUILD_TYPE=Both
    echo Build type: Both
) else (
    echo [Error] invalid input, please input 1, 2 or 3.
    goto CHOOSE_TYPE
)

:: Check toolchain availability
where gcc >nul 2>nul
if %errorlevel% neq 0 (
    echo [Error] gcc not found in PATH. Please open MinGW shell or add MinGW bin to PATH.
    goto ERROR
)

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [Error] cmake not found in PATH.
    goto ERROR
)

if /I "%BUILD_TYPE%"=="Both" (
    call :BUILD_ONE Debug
    if %errorlevel% neq 0 goto ERROR
    call :BUILD_ONE Release
    if %errorlevel% neq 0 goto ERROR
) else (
    call :BUILD_ONE %BUILD_TYPE%
    if %errorlevel% neq 0 goto ERROR
)

echo.
echo ==================================================
echo                BUILD SUCCESSFUL ^!
echo ==================================================
echo.
if /I "%BUILD_TYPE%"=="Both" (
    echo Debug files:
    echo   DLL: %BUILD_DIR%\debug\libCodroid.dll
    echo   LIB: %BUILD_DIR%\debug\libCodroid.dll.a
    echo.
    echo Release files:
    echo   DLL: %BUILD_DIR%\release\libCodroid.dll
    echo   LIB: %BUILD_DIR%\release\libCodroid.dll.a
) else if /I "%BUILD_TYPE%"=="Debug" (
    echo Debug files:
    echo   DLL: %BUILD_DIR%\debug\libCodroid.dll
    echo   LIB: %BUILD_DIR%\debug\libCodroid.dll.a
) else (
    echo Release files:
    echo   DLL: %BUILD_DIR%\release\libCodroid.dll
    echo   LIB: %BUILD_DIR%\release\libCodroid.dll.a
)
echo.
echo ==================================================
pause
exit /b 0

:BUILD_ONE
set CFG=%~1
set CFG_LC=%CFG%
if /I "%CFG%"=="Debug" set CFG_LC=debug
if /I "%CFG%"=="Release" set CFG_LC=release
set CFG_DIR=%BUILD_DIR%\%CFG_LC%

if exist "%CFG_DIR%" (
    echo [clean] Removing old %CFG% build directory...
    rd /s /q "%CFG_DIR%"
)

echo [configure] %CFG% with MinGW Makefiles...
cmake -S . -B "%CFG_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%CFG%
if %errorlevel% neq 0 exit /b %errorlevel%

echo [build] %CFG%...
cmake --build "%CFG_DIR%" -j
if %errorlevel% neq 0 exit /b %errorlevel%

exit /b 0

:ERROR
echo.
echo !!!!!!!!!!!!!!! BUILD FAILED !!!!!!!!!!!!!!!
echo.
pause
exit /b %errorlevel%
