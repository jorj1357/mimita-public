@echo off
setlocal enabledelayedexpansion

set COMPILER=C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe
set GLFW_INC=C:\important\glfw-3.4.bin.WIN64\include
set GLFW_LIB=C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64
set ROOT=C:\important\mimita-priv-v8
set MINI=%ROOT%\minimimita
set SRC=%MINI%\src
set BUILD=%MINI%\build

if not exist "%BUILD%" mkdir "%BUILD%"

set CXXFLAGS=-std=c++17 -O0 -g -pipe -MMD -MP
set INCLUDES=-I%ROOT%\include -I%SRC% -I%GLFW_INC%
set DEFINES=-DGLM_ENABLE_EXPERIMENTAL
set LIBS=-L%GLFW_LIB% -lglfw3 -lopengl32 -lgdi32 -luser32 -ldwmapi

echo === Compiling glad.c ===
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%ROOT%\src\glad.c" -o "%BUILD%\glad.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Compiling source files ===
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\main.cpp" -o "%BUILD%\main.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\globals.cpp" -o "%BUILD%\globals.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\input.cpp" -o "%BUILD%\input.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\init.cpp" -o "%BUILD%\init.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\map-utils.cpp" -o "%BUILD%\map-utils.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\physics.cpp" -o "%BUILD%\physics.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\collision-grid.cpp" -o "%BUILD%\collision-grid.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\render-core.cpp" -o "%BUILD%\render-core.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\render-primitives.cpp" -o "%BUILD%\render-primitives.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\render-scene.cpp" -o "%BUILD%\render-scene.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\maps.cpp" -o "%BUILD%\maps.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%COMPILER% %CXXFLAGS% %INCLUDES% %DEFINES% -c "%SRC%\glb-loader.cpp" -o "%BUILD%\glb-loader.o"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Linking ===
%COMPILER% "%BUILD%\glad.o" "%BUILD%\main.o" "%BUILD%\globals.o" "%BUILD%\input.o" "%BUILD%\init.o" "%BUILD%\map-utils.o" "%BUILD%\physics.o" "%BUILD%\collision-grid.o" "%BUILD%\render-core.o" "%BUILD%\render-primitives.o" "%BUILD%\render-scene.o" "%BUILD%\maps.o" "%BUILD%\glb-loader.o" %LIBS% -o "%MINI%\mini-mimita-collision.exe"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Build complete ===
if exist "%MINI%\mini-mimita-collision.exe" (
    echo SUCCESS: mini-mimita-collision.exe created
) else (
    echo FAILED: executable not found
)
