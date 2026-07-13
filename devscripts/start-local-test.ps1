# Start one MiMITA server and two CLI clients for local multiplayer testing.
# No coordinator required. All traffic over 127.0.0.1:1357.
#
# Usage:
#   powershell -File devscripts/start-local-test.ps1
#
# Or manually in three terminals:
#   Server:  mimita.exe --server --map funworld3
#   ClientA: mimita.exe --client --connect 127.0.0.1:1357 --name ClientA --map funworld3
#   ClientB: mimita.exe --client --connect 127.0.0.1:1357 --name ClientB --map funworld3

$ServerArgs = "--server --map funworld3"
$ClientAArgs = "--client --connect 127.0.0.1:1357 --name ClientA --map funworld3"
$ClientBArgs = "--client --connect 127.0.0.1:1357 --name ClientB --map funworld3"

$ExePath = Join-Path (Get-Location) "mimita.exe"

if (-not (Test-Path $ExePath)) {
    Write-Host "ERROR: mimita.exe not found in current directory." -ForegroundColor Red
    Write-Host "Build first with: python build_agent.py" -ForegroundColor Yellow
    exit 1
}

Write-Host "=== MiMITA Local Multiplayer Test ===" -ForegroundColor Cyan
Write-Host "Server:  $ServerArgs" -ForegroundColor Green
Write-Host "ClientA: $ClientAArgs" -ForegroundColor Green
Write-Host "ClientB: $ClientBArgs" -ForegroundColor Green
Write-Host ""
Write-Host "Starting server..." -ForegroundColor Yellow

$ServerProcess = Start-Process -FilePath $ExePath -ArgumentList $ServerArgs -NoNewWindow -PassThru
Start-Sleep -Milliseconds 1000

Write-Host "Starting Client A..." -ForegroundColor Yellow
$ClientAProcess = Start-Process -FilePath $ExePath -ArgumentList $ClientAArgs -PassThru

Start-Sleep -Milliseconds 500

Write-Host "Starting Client B..." -ForegroundColor Yellow
$ClientBProcess = Start-Process -FilePath $ExePath -ArgumentList $ClientBArgs -PassThru

Write-Host ""
Write-Host "=== All processes launched ===" -ForegroundColor Cyan
Write-Host "Server  PID: $($ServerProcess.Id)" -ForegroundColor Green
Write-Host "ClientA PID: $($ClientAProcess.Id)" -ForegroundColor Green
Write-Host "ClientB PID: $($ClientBProcess.Id)" -ForegroundColor Green
Write-Host ""
Write-Host "Press any key to terminate all processes..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

Write-Host "Terminating..." -ForegroundColor Red
if (-not $ServerProcess.HasExited) { $ServerProcess.Kill() }
if (-not $ClientAProcess.HasExited) { $ClientAProcess.Kill() }
if (-not $ClientBProcess.HasExited) { $ClientBProcess.Kill() }
Write-Host "Done." -ForegroundColor Green
