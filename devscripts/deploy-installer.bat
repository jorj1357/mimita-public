@echo off
REM Deploy MimitaSetup.exe to VPS
REM Usage: deploy-installer.bat [host]
REM Default host: root@107.191.48.226

set HOST=%1
if "%HOST%"=="" set HOST=root@107.191.48.226

set INSTALLER=installer\MimitaSetup-1.0.0.exe

if not exist "%INSTALLER%" (
    echo ERROR: %INSTALLER% not found. Run build_agent.py then compile installer first.
    exit /b 1
)

echo Deploying %INSTALLER% to %HOST%:/var/www/mimita/website/server/downloads/
scp "%INSTALLER%" "%HOST%:/var/www/mimita/website/server/downloads/MimitaSetup-1.0.0.exe"

if %ERRORLEVEL% neq 0 (
    echo ERROR: SCP failed
    exit /b 1
)

echo Updating symlink/reference for latest...
ssh %HOST% "ln -sf /var/www/mimita/website/server/downloads/MimitaSetup-1.0.0.exe /var/www/mimita/website/server/downloads/MimitaSetup-latest.exe"
if %ERRORLEVEL% neq 0 (
    echo WARNING: Symlink update failed (non-fatal)
)

echo Done.
