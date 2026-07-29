@echo off
REM Build everything: version -> game -> launcher -> bundle -> deploy to website
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
echo Creating mimita-game.zip for launcher download...
echo ==========================================

python devscripts\bundle-game.py
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Bundle failed
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
echo Copying files to website directory...
echo ==========================================

set /p VER=<version.txt

copy /Y manifests\%VER%.json website\server\manifests\%VER%.json
copy /Y MimitaLauncher.exe website\server\downloads\MimitaLauncher.exe

echo.
echo ==========================================
echo BUILD COMPLETE
echo ==========================================
echo Version: %VER%
echo Game: mimita.exe
echo Launcher: MimitaLauncher.exe (self-contained, ~45 MB)
echo Manifest: manifests\%VER%.json
echo Website: website\server\downloads\MimitaLauncher.exe
echo.
