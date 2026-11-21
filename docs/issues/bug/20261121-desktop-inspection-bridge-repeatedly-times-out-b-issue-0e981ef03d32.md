# issue-0e981ef03d32: Desktop inspection bridge repeatedly times out before app inventory

## Human report (verbatim, immutable)

> why its timed out  can u mak ea fix and try again and add a issue/tracker for this like regressiions maybe\ncan u do it liek with a pthon script in mimita like if u have a fail liek that, AI auto is able to make a pthon script to accomplish wahtevr its tryying to do\nall things in miita should be doable by ai  and at least figure out able by AI , can u write that as a issue or direction for AI coolaboration and agents to move toward, as well as over time getting more and more  for AI to do , instead of human , making what used to taek 10 human actions, doable in 1 human action and AI does the rest  , that is goal over time, continuous improvemnt continnous efficiency improvment


## Triage

### 2026-11-21T00:22:01.628Z — root

Summary: Determine external CUA service_info timeout root cause and implement a supported fallback or health-aware recovery for MiMITA inspection workflows; do not claim repository code can fix the external bridge until ownership is identified.
Owner: juggernaut-npc-stuck-shooting
Priority: high

## Agent discussion and suggestions

### 2026-11-21T01:25:41.178Z — root

Message: `msg-3e3f23cff0aa` (evidence)

Subject: Python fallback evidence for desktop bridge timeout issue

Python fallback to Windows APIs successfully enumerated visible MiMITA windows and captured the visible dev-loop client window without the CUA service. Run registry and events identify run 20261120_232200, server PID 33940 online, room 9K5YPND, loopback bind 127.0.0.1:24632; client PID 24476 is marked stale. Python POST to mimita.fun was blocked by HTTP 403 / Cloudflare 1010. This proves a useful local read-only fallback exists, but does not expose the server browser row or public ICE join result. The separate dev-loop control endpoint issue is issue-80dfb9ab9c5c.

## Agent suggestion

### 2026-11-21T05:51:59.964Z — rocket-impact-investigation

Existing supported fallback found (bounded review 2026-11-21T05:4xZ, agent rocket-impact-investigation): devscripts/process-snapshot.py already provides a read-only MiMITA process/build-lock inspection path. Run 'python devscripts/process-snapshot.py'; it wrote .dev/process-snapshots/process_snapshot_20261121_053848.json with 13 MiMITA-related process rows using Toolhelp32Snapshot after the PowerShell CIM query timed out after 30s, and lists lock_owner_pids_found=[30004], lock_owner_pids_not_found=[27796]. This worked while the CUA bridge was not involved, so it is a concrete Python fallback for process/window awareness rather than a new harness. Suggested smallest change: reference it from agent-coordination/agent-self-improvement as the first fallback for process inventory and have it print the scan method and limitation, so agents do not present name/PID-only data as full window/UI state. No source edits made.
