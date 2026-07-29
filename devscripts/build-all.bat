@echo off
REM Build everything: version -> game -> pack-assets -> launcher -> manifest -> installer
REM Usage: build-all.bat [release]

set BUILD_MODE=%1
if "%BUILD_MODE%"=="" set BUILD_MODE=release

echo ==========================================
echo Generating version files...
echo ==========================================

python devscripts\generate-version.py
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Version generation failed
    exit /b 1
)

echo.
echo ==========================================
echo Building Mimita Game (%BUILD_MODE%)...
echo ==========================================

python build_agent.py %BUILD_MODE%
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Game build failed
    exit /b 1
)

echo.
echo ==========================================
echo Building Mimita Launcher...
echo ==========================================

call launcher\build.bat
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Launcher build failed
    exit /b 1
)

echo.
echo ==========================================
echo Generating update manifest...
echo ==========================================

python devscripts\generate-manifest.py
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Manifest generation failed
    exit /b 1
)

echo.
echo ==========================================
echo Building Installer...
echo ==========================================

set ISCC="%LOCALAPPDATA%\Programs\Inno Setup 6\iscc.exe"
if not exist %ISCC% (
    echo [FAIL] Inno Setup not found at %ISCC%
    echo Install Inno Setup 6 from https://jrsoftware.org/isinfo.php
    exit /b 1
)

%ISCC% installer\setup.iss
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Installer build failed
    exit /b 1
)

echo.
echo ==========================================
echo Copying files to website directory...
echo ==========================================

set /p VER=<version.txt

copy /Y manifests\%VER%.json website\server\manifests\%VER%.json
copy /Y installer\MimitaSetup-%VER%.exe website\server\downloads\MimitaSetup-%VER%.exe

echo.
echo ==========================================
echo BUILD COMPLETE
echo ==========================================
echo Version: %VER%
echo Game: mimita.exe
echo Launcher: MimitaLauncher.exe
echo Installer: installer\MimitaSetup-%VER%.exe
echo Website: website\server\downloads\MimitaSetup-%VER%.exe
echo.
