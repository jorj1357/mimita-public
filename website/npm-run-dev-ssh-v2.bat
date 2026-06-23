@echo off
cd /d "%~dp0"

echo Starting SSH tunnel...
start "Mimita Tunnel" cmd /k "ssh -L 3002:localhost:3002 root@107.191.48.226"

timeout /t 1 >nul

echo Starting Vite...
start "Mimita Vite" cmd /k "npm run dev"

timeout /t 1 >nul

start http://localhost:5173