@echo off
setlocal
cd /d "%~dp0"
python build_game_dll.py
exit /b %errorlevel%
