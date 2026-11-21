# issue-f0fdd607b477: Windows isolated test runs abort because process CreationDate is converted as a string

## Human report (verbatim, immutable)

> Isolated test launch fails with "cannot verify test server process identity" before creating any server instance or gameplay journal.


## Agent suggestion

### 2026-11-21T00:25:01.219Z — fcs-npc-live-fix

Correction: this issue was created by agent observation, not a human report. The --source agent flag was omitted at creation, so the initial metadata incorrectly labels the observation quote as user-sourced; preserve the original append-only record and use this correction for attribution. Evidence is from FCS pinned test attempts 20261121_001236 and 20261121_002119 plus source inspection of _process_identity.

## Triage

### 2026-11-21T00:25:25.215Z — fcs-npc-live-fix

Summary: Fix the DMTF timestamp conversion failure in Windows isolated-runtime identity checks so pinned runtime proof can proceed.
Owner: fcs-npc-live-fix
Priority: high

## Agent suggestion

### 2026-11-21T01:03:37.651Z — fcs-npc-live-fix

Correction to prior diagnosis: Get-CimInstance Win32_Process exposes CreationDate as System.DateTime here, and the exact ToUniversalTime expression works. The observed probe took about 25.7 seconds on this workstation, while the helper times out its PowerShell subprocess at 4 seconds. Direct kernel32 OpenProcess + QueryFullProcessImageNameW + GetProcessTimes returns the existing process identity in 0.483 ms. The corrected source-owned issue is issue-58ecf44ac956; preserve this initial false lead as append-only history.

## Deferred

### 2026-11-21T01:03:58.121Z — fcs-npc-live-fix

Reason: Superseded by corrected agent finding issue-58ecf44ac956. The DMTF/string diagnosis was disproven; correction evidence is appended here. No duplicate remediation should be performed.
Until: (not set)
