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

:: Resolve gcc/g++/make from ONE MinGW bin directory (do not mix ucrt64 and mingw64)
call :RESOLVE_TOOLCHAIN
if errorlevel 1 goto ERROR

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

:RESOLVE_TOOLCHAIN
set MINGW_BIN=
set GCC=
set GXX=
set MAKE_PROG=

:: Optional override: set MINGW_BIN=C:\msys64\mingw64\bin
if defined MINGW_BIN (
    if exist "!MINGW_BIN!\gcc.exe" goto TOOLCHAIN_VALIDATE
    echo [Error] MINGW_BIN is set but gcc.exe not found: !MINGW_BIN!
    exit /b 1
)

:: Prefer MSYS2 mingw64 (recommended), then ucrt64, else first gcc on PATH
if exist "C:\msys64\mingw64\bin\gcc.exe" set "MINGW_BIN=C:\msys64\mingw64\bin"
if not defined MINGW_BIN if exist "C:\msys64\ucrt64\bin\gcc.exe" set "MINGW_BIN=C:\msys64\ucrt64\bin"
if not defined MINGW_BIN (
    for /f "delims=" %%G in ('where gcc 2^>nul') do (
        for %%D in ("%%G") do set "MINGW_BIN=%%~dpD"
        set "MINGW_BIN=!MINGW_BIN:~0,-1!"
        goto TOOLCHAIN_VALIDATE
    )
    echo [Error] gcc not found. Open MSYS2 "MinGW x86_64" shell or add mingw64\bin to PATH.
    exit /b 1
)

:TOOLCHAIN_VALIDATE
set "GCC=%MINGW_BIN%\gcc.exe"
set "GXX=%MINGW_BIN%\g++.exe"

if not exist "%GCC%" (
    echo [Error] gcc not found: %GCC%
    exit /b 1
)
if not exist "%GXX%" (
    echo [Error] g++ not found: %GXX%
    exit /b 1
)

if exist "%MINGW_BIN%\mingw32-make.exe" (
    set "MAKE_PROG=%MINGW_BIN%\mingw32-make.exe"
) else if exist "%MINGW_BIN%\make.exe" (
    set "MAKE_PROG=%MINGW_BIN%\make.exe"
) else (
    echo [Error] mingw32-make.exe / make.exe not found in: %MINGW_BIN%
    echo         MSYS2: pacman -S mingw-w64-x86_64-toolchain
    exit /b 1
)

echo [toolchain] bin: %MINGW_BIN%
echo [toolchain] gcc: %GCC%
echo [toolchain] g++: %GXX%
echo [toolchain] make: %MAKE_PROG%
exit /b 0

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
    -DCMAKE_MAKE_PROGRAM="%MAKE_PROG%" ^
    -DCMAKE_C_COMPILER="%GCC%" ^
    -DCMAKE_CXX_COMPILER="%GXX%"
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
echo Tip: use ONE MSYS2 toolchain only, e.g. C:\msys64\mingw64\bin
echo      Do not mix ucrt64 gcc with mingw64 mingw32-make in PATH.
echo.
pause
exit /b 1
