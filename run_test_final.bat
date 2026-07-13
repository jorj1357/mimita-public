@echo off
setlocal enabledelayedexpansion

REM Set the TURN password
set MIMITA_TURN_PASSWORD=XzJCOjSOCSteowuXA2Xsez/rSmbr1rAljAqFDrkUGk4=

echo Starting ICE host test...
start "ICE_HOST" /B .\mimita.exe --ice-host-test > host_out.txt 2>&1

REM Wait for room code
echo Waiting for room code...
timeout /t 8 /nobreak > nul

REM Read room code from host output 
set ROOMCODE=
for /f "tokens=2 delims==" %%a in ('findstr "code=" host_out.txt') do (
    set ROOMCODE=%%a
)

echo Room code from file: !ROOMCODE!

REM Clean the room code (remove non-alphanumeric)
set CLEANCODE=
for /f "delims=ABCDEFGHIJKLMNOPQRSTUVWXYZ23456789" %%a in ("!ROOMCODE!") do (
    set CLEANCODE=!ROOMCODE:%%a=!
)

if "!CLEANCODE!"=="" (
    echo Could not extract room code from host output
    type host_out.txt | findstr code
    taskkill /f /im mimita.exe >nul 2>&1
    exit /b 1
)

set ROOMCODE=!CLEANCODE!
echo Cleaned room code: !ROOMCODE!

echo Starting ICE join test...
.\mimita.exe --ice-join-test !ROOMCODE! > join_out.txt 2>&1

REM Wait for host to finish
timeout /t 5 /nobreak > nul

echo.
echo === HOST OUTPUT ===
type host_out.txt
echo.
echo === JOIN OUTPUT ===
type join_out.txt

findstr /i "PASS" host_out.txt > nul && (echo Host: PASS) || (echo Host: FAIL)
findstr /i "PASS" join_out.txt > nul && (echo Join: PASS) || (echo Join: FAIL)

taskkill /f /im mimita.exe >nul 2>&1
