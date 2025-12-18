rem C:\important\go away v5\s\mimita-v5\build-and-run-v2.bat

@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo === Building mimita.exe ===
C:\important\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r2\mingw64\bin\g++.exe -std=c++17 -O2 -Wall -Iinclude -Isrc -o mimita.exe src/main.cpp -lglfw3 -lopengl32 -lgdi32 -luser32

if errorlevel 1 (
    echo [ERROR] Build failed.
    pause
    exit /b
)

echo === Running mimita.exe ===
.\mimita.exe > run.log 2>&1

set ERR=%ERRORLEVEL%
echo.
echo === Program exited with code %ERR% ===
echo --- Last 20 lines of log: ---
powershell -Command "Get-Content run.log -Tail 20"
echo.
pause
