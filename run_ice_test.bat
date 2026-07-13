@echo off
set MIMITA_TURN_PASSWORD=XzJCOjSOCSteowuXA2Xsez/rSmbr1rAljAqFDrkUGk4=

echo Starting ICE Host...
start /B mimita.exe --ice-host-test > host_out.txt 2>&1

echo Waiting for room code...
timeout /t 7 /nobreak > nul

REM Extract room code from host output
for /f "tokens=2 delims==" %%a in ('findstr "code=" host_out.txt') do set ROOMCODE=%%a
set ROOMCODE=%ROOMCODE: =%
echo Room code: %ROOMCODE%

echo Starting ICE Join...
start /B mimita.exe --ice-join-test %ROOMCODE% > join_out.txt 2>&1

echo Waiting for test to complete...
timeout /t 20 /nobreak > nul

echo.
echo === RESULTS ===
echo --- HOST ---
type host_out.txt
echo.
echo --- JOIN ---
type join_out.txt

taskkill /f /im mimita.exe 2>nul
