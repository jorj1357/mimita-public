$turnPass = "XzJCOjSOCSteowuXA2Xsez/rSmbr1rAljAqFDrkUGk4="
$exePath = (Get-Item ".\mimita.exe").FullName

function Start-Proc {
    param($args)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exePath
    $psi.Arguments = $args
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.EnvironmentVariables["MIMITA_TURN_PASSWORD"] = $turnPass
    return [System.Diagnostics.Process]::Start($psi)
}

# Start host
Write-Host "Starting host..."
$hostProc = Start-Proc "--ice-host-test"

# Read host output looking for room code
$roomCode = $null
$timeout = 30000  # 30 seconds
$elapsed = 0
$hostLines = @()

while ($elapsed -lt $timeout -and -not $hostProc.HasExited) {
    Start-Sleep -Milliseconds 200
    $elapsed += 200
    # Read available data from stdout
    $line = $hostProc.StandardOutput.ReadLine()
    while ($line -ne $null) {
        $hostLines += $line
        $line = $hostProc.StandardOutput.ReadLine()
    }
    # Check for room code
    foreach ($l in $hostLines) {
        if ($l -match "code=([A-Z0-9]+)") {
            $roomCode = $Matches[1]
        }
    }
    if ($roomCode) { break }
}

if (-not $roomCode) {
    Write-Host "FAIL: No room code. Host output:"
    $hostLines | ForEach-Object { Write-Host "  $_" }
    if (-not $hostProc.HasExited) { $hostProc.Kill() }
    exit 1
}

Write-Host "Room code: $roomCode"

# Start joiner
Write-Host "Starting joiner..."
$joinProc = Start-Proc "--ice-join-test $roomCode"
$joinLines = @()

# Wait for both to finish
$timeout = 60000
$elapsed = 0
while ($elapsed -lt $timeout -and (-not $hostProc.HasExited -or -not $joinProc.HasExited)) {
    Start-Sleep -Milliseconds 200
    $elapsed += 200
    # Read host
    $line = $hostProc.StandardOutput.ReadLine()
    while ($line -ne $null) {
        $hostLines += $line
        $line = $hostProc.StandardOutput.ReadLine()
    }
    # Read join
    $line = $joinProc.StandardOutput.ReadLine()
    while ($line -ne $null) {
        $joinLines += $line
        $line = $joinProc.StandardOutput.ReadLine()
    }
}

# Final drain
Start-Sleep -Milliseconds 500
$line = $hostProc.StandardOutput.ReadLine()
while ($line -ne $null) { $hostLines += $line; $line = $hostProc.StandardOutput.ReadLine() }
$line = $joinProc.StandardOutput.ReadLine()
while ($line -ne $null) { $joinLines += $line; $line = $joinProc.StandardOutput.ReadLine() }

if (-not $hostProc.HasExited) { $hostProc.Kill() }
if (-not $joinProc.HasExited) { $joinProc.Kill() }

# Print filtered output (ICE-specific lines)
Write-Host "`n=== HOST ICE OUTPUT ==="
$hostLines | Where-Object { $_ -match "ICE|PASS|FAIL|ROOM|SIGNAL|CONNECTED|RESULT" } | ForEach-Object { Write-Host "[HOST] $_" }

Write-Host "`n=== JOIN ICE OUTPUT ==="
$joinLines | Where-Object { $_ -match "ICE|PASS|FAIL|ROOM|SIGNAL|CONNECTED|RESULT" } | ForEach-Object { Write-Host "[JOIN] $_" }

$hostAll = $hostLines -join "`n"
$joinAll = $joinLines -join "`n"
$hostOk = $hostAll -match "[HOST].*PASS|RESULT.*pass=1|PASS"
$joinOk = $joinAll -match "[JOIN].*PASS|RESULT.*pass=1|PASS"

Write-Host "`nHost PASS: $hostOk"
Write-Host "Join PASS: $joinOk"
if ($hostOk -and $joinOk) { Write-Host "OVERALL: PASS" } else { Write-Host "OVERALL: FAIL" }
