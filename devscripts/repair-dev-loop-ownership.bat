@echo off
cd /d "%~dp0.."
py -3 devscripts\repair-dev-loop-ownership.py
echo.
echo Repair finished. Read the status above before closing this window.
pause
