@echo off
cd /d "%~dp0"

REM Run mimita.exe, let the game create the log file
mimita.exe

REM After the game exits, read the latest log path
if exist "logs\latest-log-path.txt" (
    set /p LOG_PATH=<"logs\latest-log-path.txt"
    echo.
    echo ==================================================
    echo MIMITA RUN LOG SAVED
    echo %LOG_PATH%
    echo Ctrl+Click the path above to open it.
    echo ==================================================
) else (
    echo.
    echo [No log path found - check logs\latest-log-path.txt]
)

if "%1"=="/pause" (
    echo.
    pause
)
