@echo off
REM Build MimitaLauncher.exe — standalone Win32 app with GUI wizard

REM Toolchain paths are overridable via env vars (MIMITA_COMPILER / MIMITA_WINDRES)
REM so CI runners can use their own MinGW install; defaults match local dev.
if "%MIMITA_COMPILER%"=="" set "MIMITA_COMPILER=C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"
if "%MIMITA_WINDRES%"=="" set "MIMITA_WINDRES=C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\windres.exe"

set COMPILER=%MIMITA_COMPILER%
set WINDRES=%MIMITA_WINDRES%

set SRC=%~dp0main.cpp
set RC=%~dp0launcher.rc
set RES=%~dp0launcher.res.o
set OUT=%~dp0..\MimitaLauncher.exe
set MINIZ=%~dp0..\external\miniz

REM Rewrite __FILE__ so no dev path (e.g. C:\mimita-priv-v8) leaks into the
REM shipped binary — dev-path strings are a classic AV/ML false-positive signal
REM (mirrors build.py's release -fmacro-prefix-map for the game).
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "ROOT_UNIX=%ROOT:\=/%"

echo Building MimitaLauncher...

REM Compile resource file (embeds icon, loading screen, GUI config, version info)
"%WINDRES%" "%RC%" -O coff -o "%RES%"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Resource compilation failed
    exit /b 1
)

REM Compile launcher (native miniz unzip — no PowerShell, no cmd.exe)
"%COMPILER%" -std=c++17 -Os -s -mwindows -static -static-libstdc++ -static-libgcc ^
    -fmacro-prefix-map=%ROOT_UNIX%=. ^
    "%SRC%" "%MINIZ%\miniz.c" "%MINIZ%\miniz_tdef.c" "%MINIZ%\miniz_tinfl.c" "%MINIZ%\miniz_zip.c" "%RES%" -o "%OUT%" ^
    -I"%MINIZ%" -lwinhttp -lshell32 -lbcrypt -ldbghelp -lgdiplus -lole32 -luuid -lcomctl32 -ladvapi32

if %ERRORLEVEL% neq 0 (
    echo [FAIL] Launcher build failed
    exit /b 1
)

echo [OK] Launcher built: %OUT%
for %%I in ("%OUT%") do echo Size: %%~zI bytes
