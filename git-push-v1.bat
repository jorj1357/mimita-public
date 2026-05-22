@echo off
cd /d "%~dp0"

REM get timestamp
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format MM-dd-yyyy-HH-mm-ss"') do set timestamp=%%i

echo.
set /p msg=Write anything for the commit message: 

REM default message if blank
if "%msg%"=="" (
    set msg=Auto commit %timestamp%
)

git add .

git commit -m "%msg%"

git push origin main

exit