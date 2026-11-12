#!/usr/bin/env python3
"""Remove duplicate MiMITA loop owners for this checkout and restart one."""

from __future__ import annotations

import base64
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENTRY = ROOT / "devscripts" / "dev-loop.py"


def main() -> int:
    if sys.platform != "win32":
        print("This repair launcher is for Windows only.")
        return 2

    ps_script = r'''
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$repo = [IO.Path]::GetFullPath('__REPO_ROOT__').TrimEnd('\').ToLowerInvariant()
$laneRoot = $repo + '\.dev\agent-lanes\'
$all = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^pythonw?\.exe$' -and $_.CommandLine
})
$targets = foreach ($p in $all) {
    $cmd = $p.CommandLine.Replace('/', '\').ToLowerInvariant()
    if (-not $cmd.Contains($repo)) { continue }
    $role = $null
    if ($cmd.Contains(($repo + '\devscripts\dev-loop-watch.py')) -and
        ($cmd.Contains('--dev-loop-role=watchdog') -or
         $cmd.Contains('--dev-loop-role watchdog'))) {
        $role = 'root-watchdog'
    } elseif ($cmd.Contains(($repo + '\devscripts\dev-loop.py')) -and
        ($cmd.Contains('--daemon') -or
         $cmd.Contains('--dev-loop-role=daemon') -or
         $cmd.Contains('--dev-loop-role daemon'))) {
        $role = 'root-daemon'
    } elseif ($cmd.Contains(($repo + '\build.py build-only'))) {
        $role = 'root-build-worker'
    } elseif ($cmd.Contains($laneRoot + 'devscripts\dev-loop-watch.py') -and
        ($cmd.Contains('--dev-loop-role=watchdog') -or
         $cmd.Contains('--dev-loop-role watchdog'))) {
        $role = 'lane-watchdog'
    } elseif ($cmd.Contains($laneRoot + 'devscripts\dev-loop.py') -and
        ($cmd.Contains('--daemon') -or
         $cmd.Contains('--dev-loop-role=daemon') -or
         $cmd.Contains('--dev-loop-role daemon'))) {
        $role = 'lane-daemon'
    } elseif ($cmd.Contains($laneRoot) -and $cmd.Contains('\build.py build-only')) {
        $role = 'lane-build-worker'
    }
    if ($role) {
        [pscustomobject]@{ Id = [int]$p.ProcessId; Role = $role;
            Created = [string]$p.CreationDate; CommandLine = [string]$p.CommandLine }
    }
}
$targets = @($targets | Sort-Object @{Expression={
    switch -Wildcard ($_.Role) {
        '*watchdog' { 0 }
        '*daemon' { 1 }
        default { 2 }
    }
}}, Id)
foreach ($target in $targets) {
    $current = Get-CimInstance Win32_Process -Filter "ProcessId = $($target.Id)"
    if (-not $current) { continue }
    $cmd = ([string]$current.CommandLine).Replace('/', '\').ToLowerInvariant()
    $stillExact = ([string]$current.CreationDate -eq $target.Created) -and
        $cmd.Contains($repo) -and
        (($target.Role -eq 'root-watchdog' -and $cmd.Contains(($repo + '\devscripts\dev-loop-watch.py'))) -or
         ($target.Role -eq 'root-daemon' -and $cmd.Contains(($repo + '\devscripts\dev-loop.py')) -and
          ($cmd.Contains('--daemon') -or $cmd.Contains('--dev-loop-role=daemon') -or
           $cmd.Contains('--dev-loop-role daemon'))) -or
         ($target.Role -eq 'root-build-worker' -and
          $cmd.Contains(($repo + '\build.py build-only'))) -or
         ($target.Role -eq 'lane-watchdog' -and $cmd.Contains($laneRoot + 'devscripts\dev-loop-watch.py')) -or
         ($target.Role -eq 'lane-daemon' -and $cmd.Contains($laneRoot + 'devscripts\dev-loop.py') -and
          ($cmd.Contains('--daemon') -or $cmd.Contains('--dev-loop-role=daemon') -or
           $cmd.Contains('--dev-loop-role daemon'))) -or
         ($target.Role -eq 'lane-build-worker' -and $cmd.Contains($laneRoot) -and
          $cmd.Contains('\build.py build-only')))
    if (-not $stillExact) {
        Write-Output "SKIP changed/reused PID $($target.Id) [$($target.Role)]"
        continue
    }
    Write-Output "STOP PID $($target.Id) [$($target.Role)]"
    Stop-Process -Id $target.Id -Force
}
Write-Output "Stopped verified root/lane loop owners and build-only workers; lane files were preserved."
'''
    ps_script = ps_script.replace("__REPO_ROOT__", str(ROOT).replace("'", "''"))
    encoded_script = base64.b64encode(ps_script.encode("utf-16le")).decode("ascii")
    try:
        stopped = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-EncodedCommand",
             encoded_script],
            cwd=ROOT, text=True, capture_output=True, timeout=45, check=False,
        )
    except (OSError, subprocess.SubprocessError) as error:
        print(f"Process cleanup could not run: {error}")
        return 2
    if stopped.stdout:
        print(stopped.stdout.rstrip())
    if stopped.stderr:
        print(stopped.stderr.rstrip(), file=sys.stderr)
    if stopped.returncode:
        print("Cleanup failed; the canonical dev-loop was not started.")
        return stopped.returncode

    print("Starting one root dev-loop and its Python dashboard...")
    try:
        started = subprocess.run(
            [sys.executable, str(ENTRY), "--ensure-running", "--no-terminal-relaunch"],
            cwd=ROOT, check=False,
        )
    except OSError as error:
        print(f"Could not start the root dev-loop: {error}")
        return 2
    return started.returncode


if __name__ == "__main__":
    raise SystemExit(main())
