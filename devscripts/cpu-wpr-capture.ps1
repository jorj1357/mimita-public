[CmdletBinding()]
param(
    [ValidateSet('Install', 'InstallElevated', 'Update', 'UpdateElevated', 'Capture', 'Remove', 'RemoveElevated')]
    [string]$Mode = 'Install'
)

$ErrorActionPreference = 'Stop'

$script:TaskName = 'MiMITA CPU Stack Capture'
$script:InstallDirectory = Join-Path $env:ProgramFiles 'MiMITA Diagnostics'
$script:InstalledScript = Join-Path $script:InstallDirectory 'cpu-wpr-capture.ps1'
$script:InstalledLauncher = Join-Path $script:InstallDirectory 'cpu-wpr-capture.cmd'
$script:RepositoryRoot = 'C:\mimita-v9'
$script:CaptureDirectory = Join-Path $script:RepositoryRoot '.dev\perf-captures'
$script:CaptureLog = Join-Path $script:CaptureDirectory 'capture-task.jsonl'
$script:WprPath = Join-Path $env:WINDIR 'System32\wpr.exe'
$script:XperfPath = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Windows Performance Toolkit\xperf.exe'
$script:PowerShellPath = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
$script:CaptureSeconds = 12
$script:WprInstanceName = 'MiMITA_CPU_Profile'
$script:MinimumFreeBytes = 8GB
$script:MinimumFreeMemoryBytes = 2GB
$script:ReviewSizeBytes = 2GB

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    if ($identity.User.Value -eq 'S-1-5-18') {
        return $true
    }
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Write-CaptureRecord {
    param([Parameter(Mandatory = $true)][hashtable]$Record)

    if (-not (Test-Path -LiteralPath $script:CaptureDirectory)) {
        New-Item -ItemType Directory -Path $script:CaptureDirectory -Force | Out-Null
    }
    $jsonLine = $Record | ConvertTo-Json -Depth 6 -Compress
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::AppendAllText(
        $script:CaptureLog,
        $jsonLine + [Environment]::NewLine,
        $utf8NoBom)
}

function Write-CaptureLauncher {
    $launcherLines = @(
        '@echo off'
        'setlocal'
        'set "LOG=C:\mimita-v9\.dev\perf-captures\capture-launch.log"'
        'set "OUT=C:\mimita-v9\.dev\perf-captures\capture-launch-output.log"'
        '>>"%LOG%" echo task-launch-start %DATE% %TIME%'
        '"C:\WINDOWS\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "C:\Program Files\MiMITA Diagnostics\cpu-wpr-capture.ps1" -Mode Capture >>"%OUT%" 2>&1'
        'set "CAPTURE_EXIT=%ERRORLEVEL%"'
        '>>"%LOG%" echo task-launch-exit %CAPTURE_EXIT% %DATE% %TIME%'
        'exit /b %CAPTURE_EXIT%'
    )
    [System.IO.File]::WriteAllText(
        $script:InstalledLauncher,
        ($launcherLines -join "`r`n") + "`r`n",
        (New-Object System.Text.UTF8Encoding($false)))
}

function Get-AvailablePhysicalMemoryBytes {
    if (-not ('MiMITA.CaptureMemoryStatus' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

namespace MiMITA {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
    public sealed class CaptureMemoryStatusEx {
        public uint dwLength = (uint)Marshal.SizeOf(typeof(CaptureMemoryStatusEx));
        public uint dwMemoryLoad;
        public ulong ullTotalPhys;
        public ulong ullAvailPhys;
        public ulong ullTotalPageFile;
        public ulong ullAvailPageFile;
        public ulong ullTotalVirtual;
        public ulong ullAvailVirtual;
        public ulong ullAvailExtendedVirtual;
    }

    public static class CaptureMemoryStatus {
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool GlobalMemoryStatusEx(CaptureMemoryStatusEx status);
    }
}
'@
    }

    $status = New-Object MiMITA.CaptureMemoryStatusEx
    if (-not [MiMITA.CaptureMemoryStatus]::GlobalMemoryStatusEx($status)) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw ("GlobalMemoryStatusEx failed with Win32 error {0}." -f $errorCode)
    }
    return [int64]$status.ullAvailPhys
}

function Get-AvailableDiskSpaceBytes {
    $drive = New-Object System.IO.DriveInfo('C:\')
    if (-not $drive.IsReady) {
        throw 'The C: volume is not ready for a capture-space check.'
    }
    return [int64]$drive.AvailableFreeSpace
}

function Start-ElevatedMode {
    param([Parameter(Mandatory = $true)][string]$ElevatedMode)

    if (-not (Test-Path -LiteralPath $script:PowerShellPath -PathType Leaf)) {
        throw ("Windows PowerShell was not found: {0}" -f $script:PowerShellPath)
    }
    $argumentLine = '-NoLogo -NoProfile -ExecutionPolicy Bypass -File "{0}" -Mode {1}' -f $PSCommandPath, $ElevatedMode
    try {
        $process = Start-Process -FilePath $script:PowerShellPath -ArgumentList $argumentLine `
            -Verb RunAs -WindowStyle Hidden -PassThru
        Write-Output ("Started the one-time elevated {0} operation (PID {1}). Approve the Windows UAC prompt to continue." -f $ElevatedMode, $process.Id)
    }
    catch {
        throw ("Windows did not approve the elevated {0} operation: {1}" -f $ElevatedMode, $_.Exception.Message)
    }
}

function Install-CaptureTaskElevated {
    if (-not (Test-IsAdministrator)) {
        throw 'InstallElevated requires the Windows administrator token.'
    }

    $existingTask = Get-ScheduledTask -TaskName $script:TaskName -ErrorAction SilentlyContinue
    if ($existingTask) {
        throw ("Task '{0}' already exists. It was left unchanged." -f $script:TaskName)
    }
    if (Test-Path -LiteralPath $script:InstallDirectory) {
        throw ("Install directory already exists and was left untouched: {0}" -f $script:InstallDirectory)
    }

    if (-not (Test-Path -LiteralPath $PSCommandPath -PathType Leaf)) {
        throw ("Cannot locate this setup script: {0}" -f $PSCommandPath)
    }

    New-Item -ItemType Directory -Path $script:InstallDirectory | Out-Null
    Copy-Item -LiteralPath $PSCommandPath -Destination $script:InstalledScript

    $folderAcl = New-Object System.Security.AccessControl.DirectorySecurity
    $folderAcl.SetAccessRuleProtection($true, $false)
    $folderAcl.SetOwner((New-Object System.Security.Principal.NTAccount('BUILTIN', 'Administrators')))
    $inheritance = [System.Security.AccessControl.InheritanceFlags]::ContainerInherit -bor `
        [System.Security.AccessControl.InheritanceFlags]::ObjectInherit
    $noPropagation = [System.Security.AccessControl.PropagationFlags]::None
    $allow = [System.Security.AccessControl.AccessControlType]::Allow
    $adminRule = New-Object System.Security.AccessControl.FileSystemAccessRule(
        'BUILTIN\Administrators',
        [System.Security.AccessControl.FileSystemRights]::FullControl,
        $inheritance, $noPropagation, $allow)
    $systemRule = New-Object System.Security.AccessControl.FileSystemAccessRule(
        'NT AUTHORITY\SYSTEM',
        [System.Security.AccessControl.FileSystemRights]::FullControl,
        $inheritance, $noPropagation, $allow)
    $userRule = New-Object System.Security.AccessControl.FileSystemAccessRule(
        'BUILTIN\Users',
        [System.Security.AccessControl.FileSystemRights]::ReadAndExecute,
        $inheritance, $noPropagation, $allow)
    $folderAcl.AddAccessRule($adminRule)
    $folderAcl.AddAccessRule($systemRule)
    $folderAcl.AddAccessRule($userRule)
    Set-Acl -LiteralPath $script:InstallDirectory -AclObject $folderAcl
    Write-CaptureLauncher

    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $userName = $identity.Name
    $userSid = $identity.User.Value
    $arguments = '/d /c ""{0}""' -f $script:InstalledLauncher
    $taskSddl = 'D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;GRGX;;;{0})' -f $userSid

    $service = New-Object -ComObject 'Schedule.Service'
    $service.Connect()
    $rootFolder = $service.GetFolder('\')
    $definition = $service.NewTask(0)
    $definition.RegistrationInfo.Description = 'On-demand 12-second system CPU stack capture for MiMITA. Writes a local ETL after disk-space checks.'
    $definition.RegistrationInfo.Author = $userName
    $definition.Principal.UserId = 'SYSTEM'
    $definition.Principal.LogonType = 5
    $definition.Principal.RunLevel = 1
    $definition.Settings.AllowDemandStart = $true
    $definition.Settings.StartWhenAvailable = $false
    $definition.Settings.DisallowStartIfOnBatteries = $false
    $definition.Settings.StopIfGoingOnBatteries = $false
    $definition.Settings.MultipleInstances = 2
    $definition.Settings.ExecutionTimeLimit = 'PT10M'
    $definition.Settings.Hidden = $false
    $execAction = $definition.Actions.Create(0)
    $execAction.Path = Join-Path $env:WINDIR 'System32\cmd.exe'
    $execAction.Arguments = $arguments
    $execAction.WorkingDirectory = $script:RepositoryRoot

    try {
        $null = $rootFolder.RegisterTaskDefinition(
            $script:TaskName, $definition, 6, 'SYSTEM', $null, 5, $taskSddl)
    }
    catch {
        Unregister-ScheduledTask -TaskName $script:TaskName -Confirm:$false -ErrorAction SilentlyContinue
        throw
    }

    Write-CaptureRecord @{
        status = 'task-installed'
        task_name = $script:TaskName
        task_principal = 'SYSTEM'
        task_run_level = 'highest'
        script_path = $script:InstalledScript
        script_directory_acl = 'Administrators/System full control; Users read and execute'
        task_acl = 'Administrators/System full control; current user read and execute'
        capture_profile = 'CPU verbose file mode'
        wpr_instance_name = $script:WprInstanceName
        capture_seconds = $script:CaptureSeconds
        created_utc = [DateTime]::UtcNow.ToString('o')
    }
    $triggerCommand = 'schtasks /run /tn "{0}"' -f $script:TaskName
    Write-Output ("Installed on-demand task '{0}'. Trigger it with: {1}" -f $script:TaskName, $triggerCommand)
}

function Remove-CaptureTaskElevated {
    if (-not (Test-IsAdministrator)) {
        throw 'RemoveElevated requires the Windows administrator token.'
    }
    $existingTask = Get-ScheduledTask -TaskName $script:TaskName -ErrorAction SilentlyContinue
    if (-not $existingTask) {
        Write-Output ("Task '{0}' is not registered." -f $script:TaskName)
        return
    }
    if ($existingTask.State -eq 'Running') {
        throw 'The capture task is currently running; wait for its bounded capture to finish before removing it.'
    }
    Unregister-ScheduledTask -TaskName $script:TaskName -Confirm:$false
    Write-CaptureRecord @{
        status = 'task-removed'
        task_name = $script:TaskName
        removed_utc = [DateTime]::UtcNow.ToString('o')
        installed_script_preserved = $script:InstalledScript
        trace_files_preserved = $true
    }
    Write-Output ("Removed task '{0}'. Existing traces and the small protected helper file were preserved." -f $script:TaskName)
}

function Update-CaptureHelperElevated {
    if (-not (Test-IsAdministrator)) {
        throw 'UpdateElevated requires the Windows administrator token.'
    }
    $existingTask = Get-ScheduledTask -TaskName $script:TaskName -ErrorAction SilentlyContinue
    if (-not $existingTask -or -not (Test-Path -LiteralPath $script:InstalledScript -PathType Leaf)) {
        throw 'The installed capture task/helper was not found; run Install first.'
    }
    if (-not (Test-Path -LiteralPath $PSCommandPath -PathType Leaf)) {
        throw ("Cannot locate the updated setup script: {0}" -f $PSCommandPath)
    }
    $service = New-Object -ComObject 'Schedule.Service'
    $service.Connect()
    $rootFolder = $service.GetFolder('\')
    $registeredTask = $rootFolder.GetTask($script:TaskName)
    $taskSddl = $registeredTask.GetSecurityDescriptor(15)
    $definition = $registeredTask.Definition
    $definition.Principal.UserId = 'SYSTEM'
    $definition.Principal.LogonType = 5
    $definition.Principal.RunLevel = 1
    $execAction = $definition.Actions.Item(1)
    $execAction.Path = Join-Path $env:WINDIR 'System32\cmd.exe'
    $execAction.Arguments = '/d /c ""{0}""' -f $script:InstalledLauncher
    $null = $rootFolder.RegisterTaskDefinition(
        $script:TaskName, $definition, 6, 'SYSTEM', $null, 5, $taskSddl)
    $tempScript = $script:InstalledScript + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    Copy-Item -LiteralPath $PSCommandPath -Destination $tempScript
    Move-Item -LiteralPath $tempScript -Destination $script:InstalledScript -Force
    Write-CaptureLauncher
    Write-CaptureRecord @{
        status = 'helper-updated'
        task_name = $script:TaskName
        script_path = $script:InstalledScript
        task_principal = 'SYSTEM'
        task_run_level = 'highest'
        updated_utc = [DateTime]::UtcNow.ToString('o')
    }
    Write-Output ("Updated the protected helper for '{0}'." -f $script:TaskName)
}

function Invoke-WprCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Arguments,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [ValidateSet('wpr', 'xperf')][string]$ToolName = 'wpr'
    )

    $toolPath = if ($ToolName -eq 'xperf') { $script:XperfPath } else { $script:WprPath }
    $commandId = [Guid]::NewGuid().ToString('N')
    $commandStartUtc = [DateTime]::UtcNow
    Write-CaptureRecord @{
        status = ("{0}-command-launching" -f $ToolName)
        command_id = $commandId
        arguments = $Arguments
        timeout_seconds = $TimeoutSeconds
        started_utc = $commandStartUtc.ToString('o')
    }
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $toolPath
    $startInfo.Arguments = $Arguments
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw ("Could not start {0} command: {1}" -f $ToolName, $Arguments)
    }
    $processId = $process.Id
    Write-CaptureRecord @{
        status = ("{0}-command-started" -f $ToolName)
        command_id = $commandId
        arguments = $Arguments
        process_id = $processId
        timeout_seconds = $TimeoutSeconds
        started_utc = $commandStartUtc.ToString('o')
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        try { $process.Kill() } catch { }
        $exitedAfterKill = $process.WaitForExit(5000)
        $timeoutOutput = ''
        if ($stdoutTask.IsCompleted) { $timeoutOutput += $stdoutTask.Result }
        if ($stderrTask.IsCompleted) { $timeoutOutput += $stderrTask.Result }
        Write-CaptureRecord @{
            status = ("{0}-command-timeout" -f $ToolName)
            command_id = $commandId
            arguments = $Arguments
            process_id = $processId
            exited_after_kill = $exitedAfterKill
            timeout_seconds = $TimeoutSeconds
            elapsed_seconds = [math]::Round(([DateTime]::UtcNow - $commandStartUtc).TotalSeconds, 2)
            output = $timeoutOutput.Trim()
        }
        throw ("{0} command timed out after {1}s (PID {2}, exited after kill={3}): {4}" -f $ToolName, $TimeoutSeconds, $processId, $exitedAfterKill, $Arguments)
    }
    $result = @{
        exit_code = $process.ExitCode
        output = (($stdoutTask.Result + [Environment]::NewLine + $stderrTask.Result).Trim())
    }
    $loggedOutput = $result.output
    if ($ToolName -eq 'xperf' -and $Arguments -eq '-loggers') {
        $loggerNames = [regex]::Matches($result.output, '(?im)^\s*Logger Name\s*:\s*(.+?)\s*$') |
            ForEach-Object { $_.Groups[1].Value.Trim() }
        $loggedOutput = if ($loggerNames.Count -gt 0) {
            'Active loggers: ' + ($loggerNames -join ', ')
        } else {
            $result.output
        }
    }
    Write-CaptureRecord @{
        status = ("{0}-command-completed" -f $ToolName)
        command_id = $commandId
        arguments = $Arguments
        process_id = $processId
        exit_code = $result.exit_code
        elapsed_seconds = [math]::Round(([DateTime]::UtcNow - $commandStartUtc).TotalSeconds, 2)
        output = $loggedOutput
    }
    return $result
}

function Invoke-BoundedCapture {
    Write-CaptureRecord @{
        status = 'capture-entered'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
    }
    Write-CaptureRecord @{
        status = 'capture-identity-check-started'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
    }
    $isAdministrator = Test-IsAdministrator
    $captureIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    Write-CaptureRecord @{
        status = 'capture-identity-check-completed'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
        elevated = $isAdministrator
        identity = $captureIdentity.Name
        identity_sid = $captureIdentity.User.Value
    }
    if (-not $isAdministrator) {
        throw 'The capture task did not receive its configured elevated token.'
    }
    Write-CaptureRecord @{
        status = 'capture-wpr-path-check-started'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
    }
    if (-not (Test-Path -LiteralPath $script:WprPath -PathType Leaf)) {
        throw ("Windows Performance Recorder was not found: {0}" -f $script:WprPath)
    }
    if (-not (Test-Path -LiteralPath $script:CaptureDirectory)) {
        New-Item -ItemType Directory -Path $script:CaptureDirectory -Force | Out-Null
    }

    Write-CaptureRecord @{
        status = 'capture-preflight-started'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
        profile = 'CPU verbose file mode'
        wpr_instance_name = $script:WprInstanceName
    }

    Write-CaptureRecord @{
        status = 'capture-disk-check-started'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
    }
    $diskFreeBytes = Get-AvailableDiskSpaceBytes
    Write-CaptureRecord @{
        status = 'capture-disk-check-completed'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
        disk_free_bytes = $diskFreeBytes
    }
    if ($diskFreeBytes -lt $script:MinimumFreeBytes) {
        throw ("Capture refused: C: has {0:N2} GiB free; at least 8 GiB is required." -f ($diskFreeBytes / 1GB))
    }
    Write-CaptureRecord @{
        status = 'capture-memory-check-started'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
    }
    $freeMemoryBytes = Get-AvailablePhysicalMemoryBytes
    Write-CaptureRecord @{
        status = 'capture-memory-check-completed'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
        memory_free_bytes = $freeMemoryBytes
    }
    if ($freeMemoryBytes -lt $script:MinimumFreeMemoryBytes) {
        throw ("Capture refused: only {0:N0} MiB of physical RAM is free; at least 2048 MiB is required." -f ($freeMemoryBytes / 1MB))
    }
    Write-CaptureRecord @{
        status = 'capture-preflight-passed'
        task_name = $script:TaskName
        checked_utc = [DateTime]::UtcNow.ToString('o')
        disk_free_bytes = $diskFreeBytes
        memory_free_bytes = $freeMemoryBytes
    }
    $wprStatus = Invoke-WprCommand -Arguments ("-status -instancename {0}" -f $script:WprInstanceName) -TimeoutSeconds 30
    if ($wprStatus.exit_code -ne 0 -or $wprStatus.output -notmatch 'WPR is not recording') {
        throw ("Capture refused because WPR is already recording or its state is unknown: {0}" -f $wprStatus.output)
    }

    $stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
    $tracePath = Join-Path $script:CaptureDirectory ("mimita-cpu-{0}.etl" -f $stamp)
    if (Test-Path -LiteralPath $tracePath) {
        throw ("Capture refused rather than overwrite an existing trace: {0}" -f $tracePath)
    }
    $started = $false
    $usingXperf = $true
    $traceTool = 'xperf'
    $startUtc = [DateTime]::UtcNow
    $startOutput = @()
    $stopOutput = @()
    $stopExitCode = $null
    $failure = $null

    try {
        if (-not (Test-Path -LiteralPath $script:XperfPath -PathType Leaf)) {
            throw ("Windows Performance Toolkit xperf was not found: {0}" -f $script:XperfPath)
        }
        $loggerStatus = Invoke-WprCommand -ToolName xperf -Arguments '-loggers' -TimeoutSeconds 30
        $activeLoggerNames = [regex]::Matches($loggerStatus.output, '(?im)^\s*Logger Name\s*:\s*(.+?)\s*$') |
            ForEach-Object { $_.Groups[1].Value.Trim() }
        $kernelLoggerActive = $activeLoggerNames -contains 'NT Kernel Logger'
        $loggerStateKnown = $activeLoggerNames.Count -gt 0 -or $loggerStatus.output -match 'No Active Loggers'
        if ($loggerStatus.exit_code -ne 0 -or -not $loggerStateKnown -or $kernelLoggerActive) {
            throw ("Capture refused because NT Kernel Logger is already active or xperf logger state is unknown: {0}" -f ($activeLoggerNames -join ', '))
        }
        $quotedTracePath = '"{0}"' -f $tracePath
        $xperfArguments = '-on PROC_THREAD+LOADER+PROFILE -stackwalk Profile -BufferSize 64 -MinBuffers 64 -MaxBuffers 128 -MaxFile 512 -FileMode Circular -f {0}' -f $quotedTracePath
        $xperfStart = Invoke-WprCommand -ToolName xperf -Arguments $xperfArguments -TimeoutSeconds 120
        $startOutput = $xperfStart.output
        if ($xperfStart.exit_code -ne 0) {
            throw ("xperf failed to start (exit {0}): {1}" -f $xperfStart.exit_code, $startOutput)
        }
        $started = $true
        Start-Sleep -Seconds $script:CaptureSeconds
        $xperfStop = Invoke-WprCommand -ToolName xperf -Arguments ("-stop -d {0}" -f $quotedTracePath) -TimeoutSeconds 120
        $stopOutput = $xperfStop.output
        $stopExitCode = $xperfStop.exit_code
        if ($stopExitCode -ne 0) {
            throw ("xperf failed to finalize the trace (exit {0}): {1}" -f $stopExitCode, $stopOutput)
        }
        $started = $false
    }
    catch {
        $failure = $_.Exception.Message
        if ($started) {
            try {
                $cancelOutput = Invoke-WprCommand -ToolName xperf -Arguments ("-stop -d `"{0}`"" -f $tracePath) -TimeoutSeconds 30
            }
            catch { $cancelOutput = $_.Exception.Message }
            $started = $false
        }
    }

    $traceInfo = Get-Item -LiteralPath $tracePath -ErrorAction SilentlyContinue
    $record = @{
        status = if ($failure) { 'capture-failed' } elseif ($traceInfo) { 'capture-complete' } else { 'trace-missing' }
        task_name = $script:TaskName
        profile = if ($usingXperf) { 'CPU sampled stacks, bounded xperf circular file mode' } else { 'CPU.verbose file mode' }
        trace_tool = $traceTool
        capture_seconds = $script:CaptureSeconds
        started_utc = $startUtc.ToString('o')
        finalized_utc = [DateTime]::UtcNow.ToString('o')
        trace_path = $tracePath
        trace_bytes = if ($traceInfo) { $traceInfo.Length } else { 0 }
        trace_stop_exit_code = $stopExitCode
        wpr_start_output = $startOutput
        wpr_stop_output = $stopOutput
        error = $failure
        disk_free_bytes_after = Get-AvailableDiskSpaceBytes
    }
    Write-CaptureRecord $record
    if ($failure) {
        throw $failure
    }
    if (-not $traceInfo) {
        throw 'WPR reported success but no ETL file was created.'
    }
    if ($traceInfo.Length -gt $script:ReviewSizeBytes) {
        Write-Warning ("Trace is {0:N2} GiB; inspect its event-loss status before opening it in WPA." -f ($traceInfo.Length / 1GB))
    }
    Write-Output ("CPU trace saved to {0} ({1:N1} MiB)." -f $tracePath, ($traceInfo.Length / 1MB))
}

try {
switch ($Mode) {
    'Install' {
        if (Test-IsAdministrator) { Install-CaptureTaskElevated }
        else { Start-ElevatedMode -ElevatedMode 'InstallElevated' }
    }
    'InstallElevated' { Install-CaptureTaskElevated }
    'Update' {
        if (Test-IsAdministrator) { Update-CaptureHelperElevated }
        else { Start-ElevatedMode -ElevatedMode 'UpdateElevated' }
    }
    'UpdateElevated' { Update-CaptureHelperElevated }
    'Capture' { Invoke-BoundedCapture }
    'Remove' {
        if (Test-IsAdministrator) { Remove-CaptureTaskElevated }
        else { Start-ElevatedMode -ElevatedMode 'RemoveElevated' }
    }
    'RemoveElevated' { Remove-CaptureTaskElevated }
}
}
catch {
    if ($Mode -eq 'Capture' -or $Mode -eq 'UpdateElevated') {
        try {
            Write-CaptureRecord @{
                status = if ($Mode -eq 'Capture') { 'capture-refused-or-failed' } else { 'helper-update-failed' }
                task_name = $script:TaskName
                checked_utc = [DateTime]::UtcNow.ToString('o')
                error = $_.Exception.Message
                operation = $Mode
            }
        }
        catch { }
    }
    throw
}
