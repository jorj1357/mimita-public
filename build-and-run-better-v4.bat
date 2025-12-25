@echo on
echo RUNNING FILE: %~f0
title Build + Run Mimita (verbose)
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo === [STEP 1] Setting up paths ===

set "GPP="

if defined MIMITA_GPP (
    set "GPP=%MIMITA_GPP%"
)

if not defined GPP if defined MINGW_ROOT (
    set "GPP=%MINGW_ROOT%\bin\g++.exe"
)

if not defined GPP (
    echo ERROR: Could not find g++.exe
    echo MIMITA_GPP = %MIMITA_GPP%
    echo MINGW_ROOT = %MINGW_ROOT%
    pause
    exit /b 1
)

if not exist "%GPP%" (
    echo ERROR: g++.exe not found at:
    echo %GPP%
    pause
    exit /b 1
)

echo Using compiler: "%GPP%"

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

rem I KNOW THIS IS NOT CROSS PLATFORM FRIENDLIES AT ALL IM SORRT IN ADVANCE JORJ dec242025
"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe" ^
!SRC_FILES! src\glad.c ^
-std=c++17 -O2 -Wall ^
-Iinclude -Isrc ^
-LC:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\lib ^
-lglfw3 -lopengl32 -lgdi32 -luser32 -ldwmapi ^
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
