$ErrorActionPreference = 'Continue'

$site = 'mimita.fun'
$turnIp = '107.191.48.226'
$turnPort = 3478
$legacyTurnHost = 'turn.stun.mimita.fun'

function Section([string]$title) {
    Write-Host "`n=== $title ===" -ForegroundColor Cyan
}

function Result([string]$name, [string]$value) {
    Write-Host ("{0}: {1}" -f $name, $value)
}

Section 'Computer and resolver'
Result 'Computer' $env:COMPUTERNAME
Result 'Time UTC' ((Get-Date).ToUniversalTime().ToString('o'))
try {
    $dns = Get-DnsClientServerAddress -AddressFamily IPv4 |
        Where-Object { $_.ServerAddresses } |
        ForEach-Object { $_.ServerAddresses } |
        Select-Object -Unique
    Result 'DNS servers' (($dns -join ', '))
} catch { Result 'DNS servers' ('ERROR ' + $_.Exception.Message) }

foreach ($hostName in @($site, $legacyTurnHost)) {
    try {
        $answers = Resolve-DnsName $hostName -Type A -ErrorAction Stop |
            Where-Object { $_.IPAddress } |
            ForEach-Object { $_.IPAddress }
        Result ("DNS A $hostName") (($answers -join ', '))
    } catch { Result ("DNS A $hostName") ('FAIL ' + $_.Exception.Message) }
}

foreach ($resolver in @('1.1.1.1', '8.8.8.8')) {
    try {
        $answers = Resolve-DnsName $site -Type A -Server $resolver -ErrorAction Stop |
            Where-Object { $_.IPAddress } |
            ForEach-Object { $_.IPAddress }
        Result ("DNS $site via $resolver") (($answers -join ', '))
    } catch { Result ("DNS $site via $resolver") ('FAIL ' + $_.Exception.Message) }
}

Section 'HTTPS and coordinator'
try {
    $r = Invoke-WebRequest -UseBasicParsing -Uri "https://$site/" -Method Head -TimeoutSec 15
    Result 'HTTPS website' ("PASS HTTP $($r.StatusCode)")
} catch { Result 'HTTPS website' ('FAIL ' + $_.Exception.Message) }

try {
    $r = Invoke-WebRequest -UseBasicParsing -Uri "https://$site/api/coordinator/list" `
        -Method Post -ContentType 'application/json' -Body '{}' -TimeoutSec 15
    Result 'Coordinator API' ("PASS HTTP $($r.StatusCode) body=$($r.Content.Substring(0, [Math]::Min(120, $r.Content.Length)))")
} catch {
    if ($_.Exception.Response) {
        Result 'Coordinator API' ("HTTP ERROR $([int]$_.Exception.Response.StatusCode)")
    } else { Result 'Coordinator API' ('FAIL ' + $_.Exception.Message) }
}

try {
    $payload = @{ identifier = 'network-diagnostic-invalid-user-9f3b' } | ConvertTo-Json -Compress
    $r = Invoke-WebRequest -UseBasicParsing -Uri "https://$site/api/game/auth/lookup" `
        -Method Post -ContentType 'application/json' -Body $payload -TimeoutSec 15
    Result 'Account API reachability' ("PASS HTTP $($r.StatusCode) (no account was changed)")
} catch {
    if ($_.Exception.Response) {
        Result 'Account API reachability' ("HTTP ERROR $([int]$_.Exception.Response.StatusCode) (server was reached)")
    } else { Result 'Account API reachability' ('FAIL ' + $_.Exception.Message) }
}

Section 'Basic route tests'
try { Result 'Ping website' ($(ping.exe -4 -n 3 $site | Select-Object -Last 1)) }
catch { Result 'Ping website' ('FAIL ' + $_.Exception.Message) }

try {
    $tcp = Test-NetConnection $site -Port 443 -InformationLevel Quiet -WarningAction SilentlyContinue
    Result 'TCP mimita.fun:443' ($(if ($tcp) { 'PASS' } else { 'FAIL' }))
} catch { Result 'TCP mimita.fun:443' ('ERROR ' + $_.Exception.Message) }

try {
    $tcp = Test-NetConnection $turnIp -Port $turnPort -InformationLevel Quiet -WarningAction SilentlyContinue
    Result "TCP $turnIp`:$turnPort" ($(if ($tcp) { 'PASS (TCP only)' } else { 'FAIL (TCP only)' }))
} catch { Result "TCP $turnIp`:$turnPort" ('ERROR ' + $_.Exception.Message) }

Section 'UDP STUN binding test'
try {
    # STUN Binding Request: type 0x0001, zero attributes, RFC 5389 magic cookie,
    # followed by a random 96-bit transaction ID.
    $bytes = [byte[]](0,1,0,0,0x21,0x12,0xA4,0x42) + [byte[]](1..12 | ForEach-Object { Get-Random -Minimum 0 -Maximum 256 })
    $udp = [System.Net.Sockets.UdpClient]::new()
    $udp.Client.ReceiveTimeout = 4000
    [void]$udp.Send($bytes, $bytes.Length, $turnIp, $turnPort)
    $remote = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
    $reply = $udp.Receive([ref]$remote)
    $udp.Dispose()
    if ($reply.Length -ge 20 -and $reply[0] -eq 1 -and $reply[1] -eq 1) {
        Result 'UDP STUN binding' ("PASS reply from $($remote.Address):$($remote.Port), bytes=$($reply.Length)")
    } else {
        Result 'UDP STUN binding' ("REPLY RECEIVED but not a normal STUN success, bytes=$($reply.Length)")
    }
} catch {
    if ($udp) { $udp.Dispose() }
    Result 'UDP STUN binding' ('FAIL/no reply ' + $_.Exception.Message)
}

Section 'Interpretation'
Write-Host 'DNS FAIL means the computer could not turn a name into an address.'
Write-Host 'HTTPS FAIL means the website/API could not be reached from this network.'
Write-Host 'Ping FAIL alone is inconclusive because many servers block ICMP.'
Write-Host 'UDP STUN FAIL is important: it can prevent direct/relay ICE connection.'
Write-Host 'A PASS here proves only this network at this moment, not every network.'
