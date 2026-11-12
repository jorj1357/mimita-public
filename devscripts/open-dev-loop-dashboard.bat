@echo off
cd /d "%~dp0.."
py -3 devscripts\dev-loop.py --ensure-running --no-terminal-relaunch
pause
