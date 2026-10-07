@echo off
setlocal
cd /d "%~dp0\.."
echo MiMITA network diagnostic
echo This checks DNS, HTTPS, the coordinator API, ICMP, TCP, and UDP STUN.
echo It does not send a password or attempt a real sign-in.
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-mimita-network.ps1"
echo.
echo Finished. Send the entire output to the MiMITA developer.
pause
