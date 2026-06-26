@echo off
REM Build everything: game + installer
REM Usage: build-all.bat [release]

echo ==========================================
echo Building Mimita Game...
echo ==========================================

python build_agent.py
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Game build failed
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
echo Copying installer to website downloads...
echo ==========================================

copy /Y installer\MimitaSetup-1.0.0.exe website\server\downloads\MimitaSetup-1.0.0.exe

echo.
echo ==========================================
echo BUILD COMPLETE
echo ==========================================
echo Game: mimita.exe
echo Installer: installer\MimitaSetup-1.0.0.exe
echo Website: website\server\downloads\MimitaSetup-1.0.0.exe
echo.
