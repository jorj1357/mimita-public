@echo on
title Build + Run Mimita (verbose)
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo === [STEP 1] Setting up paths ===

REM Check for environment variables
if defined MIMITA_GPP (
    set "GPP=%MIMITA_GPP%"
) else if defined MINGW_ROOT (
    set "GPP=%MINGW_ROOT%\bin\g++.exe"
) else (
    echo ERROR: Could not find g++.exe
    echo Please set one of the following environment variables:
    echo   MIMITA_GPP - Full path to g++.exe (e.g., C:\mingw64\bin\g++.exe)
    echo   MINGW_ROOT - Path to MinGW root directory (e.g., C:\mingw64)
    pause
    exit /b 1
)

if defined MIMITA_GLFW_INC (
    set "GLFW_INC=%MIMITA_GLFW_INC%"
) else if defined GLFW_ROOT (
    set "GLFW_INC=%GLFW_ROOT%\include"
) else (
    echo WARNING: GLFW include directory not found.
    echo Please set one of the following environment variables:
    echo   MIMITA_GLFW_INC - Full path to GLFW include directory
    echo   GLFW_ROOT - Path to GLFW root directory
    echo Attempting to continue without explicit GLFW include path...
    set "GLFW_INC="
)

if defined MIMITA_GLFW_LIB (
    set "GLFW_LIB=%MIMITA_GLFW_LIB%"
) else if defined GLFW_ROOT (
    set "GLFW_LIB=%GLFW_ROOT%\lib-mingw-w64"
) else (
    echo WARNING: GLFW library directory not found.
    echo Please set one of the following environment variables:
    echo   MIMITA_GLFW_LIB - Full path to GLFW library directory
    echo   GLFW_ROOT - Path to GLFW root directory
    echo Attempting to continue without explicit GLFW library path...
    set "GLFW_LIB="
)

set "EXT_INC=external\include"
set "EXT_LIB=external\lib"

echo Using compiler: %GPP%
if defined GLFW_INC echo GLFW include:   %GLFW_INC%
if defined GLFW_LIB echo GLFW lib:       %GLFW_LIB%
echo External include: %EXT_INC%
echo External lib:     %EXT_LIB%
echo.

echo === [STEP 2] Gathering .cpp files ===
set SRC_FILES=
for /R src %%f in (*.cpp) do (
    echo Found: %%f
    set SRC_FILES=!SRC_FILES! "%%f"
)
echo.
echo Total source files collected:
echo !SRC_FILES!
echo.

echo === [STEP 3] Building mimita.exe ===
set "INCLUDE_FLAGS=-Iinclude -Isrc -I%EXT_INC%"
if defined GLFW_INC set "INCLUDE_FLAGS=%INCLUDE_FLAGS% -I%GLFW_INC%"

set "LIB_FLAGS=-L%EXT_LIB%"
if defined GLFW_LIB set "LIB_FLAGS=%LIB_FLAGS% -L%GLFW_LIB%"

"%GPP%" !SRC_FILES! src\glad.c ^
 -std=c++17 -O2 -Wall ^
 %INCLUDE_FLAGS% ^
 %LIB_FLAGS% ^
 -lglfw3dll -lopengl32 -lgdi32 -luser32 -ldwmapi ^
 -o mimita.exe -mconsole

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed with code %errorlevel%.
    pause
    exit /b
)

echo.
echo === [STEP 4] Build successful ===
echo.

echo === [STEP 5] Running mimita.exe ===
echo (Output will be written to run.log)
echo.

.\mimita.exe > run.log 2>&1
set "ERR=%ERRORLEVEL%"

echo.
echo === Program exited with code %ERR% ===
echo --- Last 40 lines of run.log ---
if exist "run.log" (
    powershell -Command "Get-Content -Path 'run.log' -Tail 40" 2>nul || type run.log | more
) else (
    echo No run.log file found.
)
echo.
pause
endlocal
