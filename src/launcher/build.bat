@echo off
REM Build MimitaLauncher.exe
REM Standalone Win32 app — no game dependencies
REM Statically linked — zero DLL dependencies beyond system DLLs

set COMPILER=C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe

set SRC=%~dp0main.cpp
set OUT=%~dp0..\..\MimitaLauncher.exe

echo Building MimitaLauncher...
"%COMPILER%" -std=c++17 -Os -s -mwindows ^
    -static -static-libstdc++ -static-libgcc ^
    -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic ^
    "%SRC%" -o "%OUT%" -lwinhttp -lshell32

if %ERRORLEVEL% neq 0 (
    echo [FAIL] Launcher build failed
    exit /b 1
)

echo [OK] Launcher built: %OUT%
for %%I in ("%OUT%") do echo Size: %%~zI bytes
