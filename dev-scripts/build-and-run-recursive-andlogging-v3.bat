@echo off
title Build and Run Mimita
setlocal enabledelayedexpansion
cd /d "%~dp0"

:: paths
set GPP=C:\important\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r2\mingw64\bin\g++.exe
set GLFW_INC=C:\important\glfw-3.4.bin.WIN64\include
set GLFW_LIB=C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64
set EXT_INC=external\include
set EXT_LIB=external\lib

:: gather source files
set SRC_FILES=
for /R src %%f in (*.cpp) do set SRC_FILES=!SRC_FILES! "%%f"

echo === Building mimita.exe ===
"%GPP%" %SRC_FILES% src\glad.c ^
 -std=c++17 -O2 -Wall ^
 -Iinclude -Isrc -I%EXT_INC% -I%GLFW_INC% ^
 -L%EXT_LIB% -L%GLFW_LIB% ^
 -lglfw3 -lopengl32 -lgdi32 -luser32 -ldwmapi ^
 -static-libstdc++ -static-libgcc -static ^
 -o mimita.exe -mconsole

if errorlevel 1 (
    echo [ERROR] Build failed.
    pause
    exit /b
)

echo === Build successful ===
.\mimita.exe > run.log 2>&1
set ERR=%ERRORLEVEL%

echo.
echo === Program exited with code %ERR% ===
echo --- Last 20 lines of log: ---
powershell -Command "Get-Content run.log -Tail 20"
echo.
pause
