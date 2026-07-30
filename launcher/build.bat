@echo off
REM Build MimitaLauncher.exe — standalone Win32 app with GUI wizard

set COMPILER=C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe
set WINDRES=C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\windres.exe

set SRC=%~dp0main.cpp
set RC=%~dp0launcher.rc
set RES=%~dp0launcher.res.o
set OUT=%~dp0..\MimitaLauncher.exe

echo Building MimitaLauncher...

REM Compile resource file (embeds loading screen image)
"%WINDRES%" "%RC%" -O coff -o "%RES%"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Resource compilation failed
    exit /b 1
)

REM Compile launcher
"%COMPILER%" -std=c++17 -Os -s -mwindows -static -static-libstdc++ -static-libgcc ^
    "%SRC%" "%RES%" -o "%OUT%" ^
    -lwinhttp -lshell32 -lbcrypt -ldbghelp -lgdiplus -lole32 -luuid -lcomctl32

if %ERRORLEVEL% neq 0 (
    echo [FAIL] Launcher build failed
    exit /b 1
)

echo [OK] Launcher built: %OUT%
for %%I in ("%OUT%") do echo Size: %%~zI bytes
