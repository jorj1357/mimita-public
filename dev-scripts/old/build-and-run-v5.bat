@echo off
title Build + Run Mimita (fast)
setlocal EnableDelayedExpansion
cd /d "%~dp0"

REM ---------------- CONFIG ----------------
set "GPP=C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"
set "GLFW_INC=C:\important\glfw-3.4.bin.WIN64\include"
set "GLFW_LIB=C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64"
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

set INCLUDES=-Iinclude -Isrc -I%EXT_INC% -I%GLFW_INC%
set LIBS=-L%EXT_LIB% -L%GLFW_LIB% -lglfw3dll -lopengl32 -lgdi32 -luser32 -ldwmapi

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
"C:\important\PowerShell\7\pwsh.exe" -Command "Get-Content -Path 'run.log' -Tail 40"

echo.
pause
endlocal
