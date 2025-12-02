@echo off
setlocal enabledelayedexpansion

cd /d C:\important\go away v5\s\mimita-v5

:: Check if commit message was passed in
if "%~1"=="" (
    for /f "tokens=1-4 delims=/ " %%a in ('date /t') do (
        set d=%%a-%%b-%%c
    )
    for /f "tokens=1-2 delims=: " %%a in ('time /t') do (
        set t=%%a-%%b
    )
    set msg=Auto commit !d!_!t!
) else (
    set msg=%*
)

git add .
git commit -m "!msg!"
git push

pause
