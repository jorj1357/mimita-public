# Cross-network ICE test
# This script tests ICE connectivity between two different machines/networks.
#
# Option 1: Host on your PC, client via SSH on VPS (requires Wine on VPS):
#   .\test_cross_network.ps1 -Duration 60
#
# Option 2: Manual test (recommended):
#   Server: mimita.exe --ice-server --timeout-seconds 600
#   Client (another PC): mimita.exe --ice-connect <ROOMCODE>
#
# Option 3: Host-only mode (share code with a friend):
#   .\test_cross_network.ps1 -HostOnly -Duration 300

param(
    [string]$TurnPass = "XzJCOjSOCSteowuXA2Xsez/rSmbr1rAljAqFDrkUGk4=",
    [int]$Duration = 60,
    [switch]$DisableRelay,
    [switch]$HostOnly
)

Stop-Process -Name "mimita" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

$env:MIMITA_TURN_PASSWORD = $TurnPass
$exePath = (Get-Item ".\mimita.exe").FullName
$relayFlag = if ($DisableRelay) { "--disable-relay" } else { "" }

Write-Host "Starting ICE server..."
$hostJob = Start-Job -ScriptBlock { param($e, $p, $dur, $rf) $env:MIMITA_TURN_PASSWORD=$p; & $e "--ice-server" "--timeout-seconds" $dur $rf 2>&1 } -ArgumentList $exePath, $TurnPass, $Duration, $relayFlag

Start-Sleep -Seconds 10
$partial = Receive-Job -Job $hostJob -Keep 2>&1 | Out-String
$roomCode = if ($partial -match "Room Code: (\w+)") { $Matches[1] } else { "" }

if (-not $roomCode) { Write-Host "FAIL: No room code"; Stop-Job $hostJob; exit 1 }

Write-Host "`n============================================"
Write-Host "  ROOM CODE: $roomCode"
Write-Host "  Join: mimita.exe --ice-connect $roomCode"
Write-Host "============================================`n"

if ($HostOnly) {
    Write-Host "Waiting $Duration seconds for remote player..."
    Start-Sleep -Seconds $Duration
    Receive-Job -Job $hostJob 2>&1 | Select-String -Pattern "SELECTED|CONNECTED|JOIN|ERROR|10040" | ForEach-Object { Write-Host "  $_" }
    Stop-Job $hostJob
    exit 0
}

Write-Host "Running client via SSH + Wine on VPS..."
ssh root@107.191.48.226 "WINEPATH=/root WINEDLLOVERRIDES='winhttp,winhttp=n' timeout $Duration wine64 Z:\\root\\mimita.exe --ice-connect $roomCode $relayFlag 2>&1" 2>&1 | ForEach-Object { Write-Host "[VPS] $_" }

Start-Sleep -Seconds 5
$hostOutput = Receive-Job -Job $hostJob 2>&1 | Out-String
Stop-Job $hostJob -ErrorAction SilentlyContinue

$hostPass = $hostOutput -match "SELECTED PATH"
$hostOutput | Select-String -Pattern "SELECTED|CONNECTED|JOIN|ACCEPT|SNAPSHOT|ERROR|10040" | ForEach-Object { Write-Host "[HOST] $_" }
Write-Host "`nDone - check SELECTED PATH for candidate type (host/srflx/relay)"
