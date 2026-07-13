$turnPass = "XzJCOjSOCSteowuXA2Xsez/rSmbr1rAljAqFDrkUGk4="
$exePath = (Get-Item ".\mimita.exe").FullName

function Start-AsyncProc {
    param($args)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exePath
    $psi.Arguments = $args
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.EnvironmentVariables["MIMITA_TURN_PASSWORD"] = $turnPass
    $proc = [System.Diagnostics.Process]::Start($psi)
    return $proc
}

# Start host
Write-Host "Starting host..."
$hostProc = Start-AsyncProc "--ice-host-test"

# Read host output line by line with timeout for room code
$roomCode = $null
$timeout = 20000  # 20 seconds
$elapsed = 0
$hostLines = New-Object System.Collections.ArrayList

while ($elapsed -lt $timeout -and -not $roomCode -and -not $hostProc.HasExited) {
    Start-Sleep -Milliseconds 100
    $elapsed += 100
    # Read available lines without blocking
    $line = $hostProc.StandardOutput.ReadLine()
    while ($line -ne $null) {
        [void]$hostLines.Add($line)
        Write-Host "[HOST] $line"
        if ($line -match "code=(\w+)") {
            $roomCode = $Matches[1]
        }
        $line = $hostProc.StandardOutput.ReadLine()
    }
}

if (-not $roomCode) {
    Write-Host "FAIL: No room code received"
    $hostLines | ForEach-Object { Write-Host "[HOST] $_" }
    if (-not $hostProc.HasExited) { $hostProc.Kill() }
    exit 1
}

Write-Host "`nRoom code: $roomCode"

# Start joiner  
Write-Host "`nStarting joiner..."
$joinProc = Start-AsyncProc "--ice-join-test $roomCode"
$joinLines = New-Object System.Collections.ArrayList

$timeout = 60000
$elapsed = 0
while ($elapsed -lt $timeout -and (-not $hostProc.HasExited -or -not $joinProc.HasExited)) {
    Start-Sleep -Milliseconds 100
    $elapsed += 100
    
    # Read host lines
    $line = $hostProc.StandardOutput.ReadLine()
    while ($line -ne $null) {
        [void]$hostLines.Add($line)
        Write-Host "[HOST] $line"
        $line = $hostProc.StandardOutput.ReadLine()
    }
    
    # Read join lines
    $line = $joinProc.StandardOutput.ReadLine()
    while ($line -ne $null) {
        [void]$joinLines.Add($line)
        Write-Host "[JOIN] $line"
        $line = $joinProc.StandardOutput.ReadLine()
    }
}

# Final drain
Start-Sleep -Milliseconds 500
$line = $hostProc.StandardOutput.ReadLine()
while ($line -ne $null) {
    [void]$hostLines.Add($line)
    Write-Host "[HOST] $line"
    $line = $hostProc.StandardOutput.ReadLine()
}
$line = $joinProc.StandardOutput.ReadLine()
while ($line -ne $null) {
    [void]$joinLines.Add($line)
    Write-Host "[JOIN] $line"
    $line = $joinProc.StandardOutput.ReadLine()
}

if (-not $hostProc.HasExited) { $hostProc.Kill() }
if (-not $joinProc.HasExited) { $joinProc.Kill() }

$hostAll = $hostLines -join "`n"
$joinAll = $joinLines -join "`n"
$hostOk = $hostAll -match "PASS"
$joinOk = $joinAll -match "PASS"

Write-Host "`n=== RESULTS ==="
Write-Host "Host PASS: $hostOk"
Write-Host "Join PASS: $joinOk"
if ($hostOk -and $joinOk) { Write-Host "OVERALL: PASS" } else { Write-Host "OVERALL: FAIL" }
