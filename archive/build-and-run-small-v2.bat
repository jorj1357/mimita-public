@echo off
setlocal EnableDelayedExpansion

echo === Building Mimita ===

set ROOT=%~dp0
cd /d %ROOT%

set COMPILER=C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe

set SRC_FILES=
for /R src %%f in (*.cpp) do set SRC_FILES=!SRC_FILES! %%f
set SRC_FILES=!SRC_FILES! src\glad.c

set INCLUDE_FLAGS=-Iinclude -Isrc -IC:\important\glfw-3.4.bin.WIN64\include
set LIB_FLAGS=-LC:\important\glfw-3.4.bin.WIN64\lib-mingw-w64

echo Compiling...
"%COMPILER%" !SRC_FILES! -std=c++17 -O2 -Wall ^
%INCLUDE_FLAGS% ^
%LIB_FLAGS% ^
-lglfw3 -lopengl32 -lgdi32 -luser32 -ldwmapi ^
-o mimita.exe

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    pause
    exit /b
)

echo.
echo Build OK
echo Running...

mimita.exe

pause
