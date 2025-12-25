@echo off
title Build + Run Mimita (fast)
setlocal EnableDelayedExpansion
cd /d "%~dp0"

REM ---------------- CONFIG ----------------
REM Check for environment variables first, then try common locations
if defined MIMITA_GPP (
    set "GPP=%MIMITA_GPP%"
) else if defined MINGW_ROOT (
    set "GPP=%MINGW_ROOT%\bin\g++.exe"
) else (
    REM Try common MinGW installation paths
    if exist "C:\mingw64\bin\g++.exe" (
        set "GPP=C:\mingw64\bin\g++.exe"
    ) else if exist "C:\msys64\mingw64\bin\g++.exe" (
        set "GPP=C:\msys64\mingw64\bin\g++.exe"
    ) else (
        echo ERROR: Could not find g++.exe
        echo Please set MIMITA_GPP environment variable or install MinGW-w64
        pause
        exit /b 1
    )
)

if defined MIMITA_GLFW_INC (
    set "GLFW_INC=%MIMITA_GLFW_INC%"
) else if defined GLFW_ROOT (
    set "GLFW_INC=%GLFW_ROOT%\include"
) else (
    REM Try common GLFW installation paths
    if exist "C:\glfw\include" (
        set "GLFW_INC=C:\glfw\include"
    ) else if exist "C:\Program Files\GLFW\include" (
        set "GLFW_INC=C:\Program Files\GLFW\include"
    ) else (
        set "GLFW_INC="
    )
)

if defined MIMITA_GLFW_LIB (
    set "GLFW_LIB=%MIMITA_GLFW_LIB%"
) else if defined GLFW_ROOT (
    set "GLFW_LIB=%GLFW_ROOT%\lib-mingw-w64"
) else (
    REM Try common GLFW library paths
    if exist "C:\glfw\lib-mingw-w64" (
        set "GLFW_LIB=C:\glfw\lib-mingw-w64"
    ) else if exist "C:\Program Files\GLFW\lib-mingw-w64" (
        set "GLFW_LIB=C:\Program Files\GLFW\lib-mingw-w64"
    ) else (
        set "GLFW_LIB="
    )
)

set "EXT_INC=external\include"
set "EXT_LIB=external\lib"

set BUILD_DIR=build
set EXE=mimita.exe

REM debug | release
set BUILD=release

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM ---------------- FLAGS ----------------
if "%BUILD%"=="debug" (
    set CFLAGS=-std=c++17 -O0 -g -Wall -pipe
) else (
    set CFLAGS=-std=c++17 -O2 -Wall -pipe
)

set "INCLUDES=-Iinclude -Isrc -I%EXT_INC%"
if defined GLFW_INC set "INCLUDES=%INCLUDES% -I%GLFW_INC%"

set "LIBS=-L%EXT_LIB%"
if defined GLFW_LIB set "LIBS=%LIBS% -L%GLFW_LIB%"
set "LIBS=%LIBS% -lglfw3dll -lopengl32 -lgdi32 -luser32 -ldwmapi"

echo.
echo === Building Mimita (%BUILD%) ===
echo.

REM ---------------- COMPILE glad.c ----------------
if not exist %BUILD_DIR%\glad.o (
    echo Compiling glad.c
    "%GPP%" -c src\glad.c -o %BUILD_DIR%\glad.o %CFLAGS% %INCLUDES%
)

REM ---------------- COMPILE cpp files ----------------
set OBJ_FILES=%BUILD_DIR%\glad.o

for /R src %%f in (*.cpp) do (
    set OBJ=%BUILD_DIR%\%%~pnf.o
    set OBJ=!OBJ:\=!
    set OBJ=!OBJ::=!
    set OBJ=!OBJ!.o
    set OBJ_FILES=!OBJ_FILES! !OBJ!

    if not exist "!OBJ!" (
        echo Compiling %%f
        "%GPP%" -c "%%f" -o "!OBJ!" %CFLAGS% %INCLUDES%
    )
)

REM ---------------- LINK ----------------
echo Linking...
"%GPP%" %OBJ_FILES% -o %EXE% %LIBS%

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    pause
    exit /b
)

echo.
echo === Build OK ===
echo.

REM ---------------- RUN ----------------
echo Running %EXE% ...
echo.

%EXE% > run.log 2>&1
set ERR=%ERRORLEVEL%

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
