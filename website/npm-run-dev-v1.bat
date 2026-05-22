@echo off
cd /d "%~dp0"

echo Starting new Vite server...

start "" cmd /c "npm run dev"

start http://localhost:5173

exit