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

:: Check MinGW toolchain (CMake MinGW Makefiles needs gcc, g++, and make)
call :CHECK_TOOL gcc
if errorlevel 1 goto ERROR
call :CHECK_TOOL g++
if errorlevel 1 goto ERROR

set MAKE_PROG=
where mingw32-make >nul 2>nul
if not errorlevel 1 (
    for /f "delims=" %%M in ('where mingw32-make 2^>nul') do (
        set MAKE_PROG=%%M
        goto MAKE_FOUND
    )
)
where make >nul 2>nul
if not errorlevel 1 (
    for /f "delims=" %%M in ('where make 2^>nul') do (
        set MAKE_PROG=%%M
        goto MAKE_FOUND
    )
)
echo [Error] mingw32-make or make not found in PATH.
echo         Add MinGW-w64 bin directory to PATH, e.g. MSYS2: mingw64\bin
goto ERROR

:MAKE_FOUND
echo [toolchain] make: !MAKE_PROG!

where cmake >nul 2>nul
if errorlevel 1 (
    echo [Error] cmake not found in PATH.
    goto ERROR
)

if /I "%BUILD_TYPE%"=="Both" (
    call :BUILD_ONE Debug
    if errorlevel 1 goto ERROR
    call :BUILD_ONE Release
    if errorlevel 1 goto ERROR
) else (
    call :BUILD_ONE %BUILD_TYPE%
    if errorlevel 1 goto ERROR
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

:CHECK_TOOL
where %~1 >nul 2>nul
if errorlevel 1 (
    echo [Error] %~1 not found in PATH. Open "MinGW64" / "MSYS2 MinGW" shell or add its bin to PATH.
    exit /b 1
)
for /f "delims=" %%T in ('where %~1 2^>nul') do (
    echo [toolchain] %~1: %%T
    exit /b 0
)
echo [Error] %~1 not found in PATH.
exit /b 1

:BUILD_ONE
set CFG=%~1
set CFG_LC=%CFG%
if /I "%CFG%"=="Debug" set CFG_LC=debug
if /I "%CFG%"=="Release" set CFG_LC=release
set CFG_DIR=%BUILD_DIR%\%CFG_LC%
set OUT_DLL=%CFG_DIR%\libCodroid.dll

if exist "%CFG_DIR%" (
    echo [clean] Removing old %CFG% build directory...
    rd /s /q "%CFG_DIR%"
)

echo [configure] %CFG% with MinGW Makefiles...
cmake -S . -B "%CFG_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%CFG% ^
    -DCMAKE_MAKE_PROGRAM="!MAKE_PROG!" ^
    -DCMAKE_C_COMPILER=gcc ^
    -DCMAKE_CXX_COMPILER=g++
if errorlevel 1 exit /b 1

echo [build] %CFG%...
cmake --build "%CFG_DIR%" -j
if errorlevel 1 exit /b 1

if not exist "%OUT_DLL%" (
    echo [Error] expected output missing: %OUT_DLL%
    exit /b 1
)

echo [ok] %OUT_DLL%
exit /b 0

:ERROR
echo.
echo !!!!!!!!!!!!!!! BUILD FAILED !!!!!!!!!!!!!!!
echo.
pause
exit /b 1
