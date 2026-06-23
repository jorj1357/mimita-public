@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0"

echo.
echo =====================================
echo Current Branch:
for /f %%i in ('git branch --show-current') do set CURRENT_BRANCH=%%i
echo    !CURRENT_BRANCH!
echo =====================================
echo.

echo Recent Branches:
echo.

set COUNT=0

for /f "tokens=*" %%i in ('git for-each-ref --sort=-committerdate --format="%%(refname:short)" refs/heads') do (
    set /a COUNT+=1
    set BRANCH!COUNT!=%%i
    echo !COUNT!^) %%i
    if !COUNT! GEQ 9 goto :branchesdone
)

:branchesdone

echo.
set /p PICK=Pick branch (1-9) or press Enter to stay on current:

if not "%PICK%"=="" (
    call set TARGET=%%BRANCH%PICK%%%

    if not "!TARGET!"=="" (
        echo.
        echo Switching to !TARGET!
        git checkout !TARGET!
        set CURRENT_BRANCH=!TARGET!
    )
)

echo.
echo Active branch: !CURRENT_BRANCH!
echo.

set /p MSG=Commit message:

if "%MSG%"=="" (
    for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format MM-dd-yyyy-HH-mm-ss"') do set MSG=Auto commit %%i
)

git add .

git commit -m "%MSG%"

git push

echo.
echo Done.
pause