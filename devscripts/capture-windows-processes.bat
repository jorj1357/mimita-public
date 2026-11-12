@echo off
setlocal
cd /d "%~dp0.."
py -3 "%~dp0process-snapshot.py"
if errorlevel 1 (
  echo Process snapshot failed with exit code %errorlevel%.
)
echo.
echo The JSON report is in .dev\process-snapshots\
pause
