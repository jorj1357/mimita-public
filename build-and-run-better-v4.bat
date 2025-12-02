@echo on
title Build + Run Mimita (verbose)
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo === [STEP 1] Setting up paths ===
set "GPP=C:\important\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r2\mingw64\bin\g++.exe"
set "GLFW_INC=C:\important\glfw-3.4.bin.WIN64\include"
set "GLFW_LIB=C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64"
set "EXT_INC=external\include"
set "EXT_LIB=external\lib"

echo Using compiler: %GPP%
echo GLFW include:   %GLFW_INC%
echo GLFW lib:       %GLFW_LIB%
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
"%GPP%" !SRC_FILES! src\glad.c ^
 -std=c++17 -O2 -Wall ^
 -Iinclude -Isrc -I%EXT_INC% -I%GLFW_INC% ^
 -L%EXT_LIB% -L%GLFW_LIB% ^
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
"C:\important\PowerShell\7\pwsh.exe" -Command "Get-Content -Path 'run.log' -Tail 40"
echo.
pause
endlocal
