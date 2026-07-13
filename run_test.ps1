$pwd = "XzJCOjSOCSteowuXA2Xsez/rSmbr1rAljAqFDrkUGk4="

function Start-AndRead {
    param($args)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = (Get-Item ".\mimita.exe").FullName
    $psi.Arguments = $args
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.EnvironmentVariables["MIMITA_TURN_PASSWORD"] = $pwd
    $proc = [System.Diagnostics.Process]::Start($psi)
    $output = $proc.StandardOutput.ReadToEnd()
    $proc.WaitForExit()
    return $output
}

Write-Host "Starting host..."
$hostOutput = Start-AndRead "--ice-host-test"
Write-Host "=== HOST OUTPUT ==="
Write-Host $hostOutput

$roomCode = $null
if ($hostOutput -match "code=([A-Z0-9]+)") {
    $roomCode = $Matches[1]
    Write-Host "Room code: $roomCode"
} else {
    Write-Host "FAIL: No room code"
    exit 1
}

Write-Host "Starting joiner..."
$joinOutput = Start-AndRead "--ice-join-test $roomCode"
Write-Host "=== JOIN OUTPUT ==="
Write-Host $joinOutput

$hostOk = $hostOutput -match "PASS|pass=1"
$joinOk = $joinOutput -match "PASS|pass=1"
Write-Host "Host PASS: $hostOk"
Write-Host "Join PASS: $joinOk"
Write-Host "OVERALL: $(if ($hostOk -and $joinOk) { "PASS" } else { "FAIL" })"
