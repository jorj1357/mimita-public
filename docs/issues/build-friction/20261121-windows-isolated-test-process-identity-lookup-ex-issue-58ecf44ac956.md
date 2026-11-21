# issue-58ecf44ac956: Windows isolated test process identity lookup exceeds its four-second shell timeout

## Human report (verbatim, immutable)

> A valid identity probe for an existing MiMITA PID returned no result inside the isolated runner because the PowerShell/WMI subprocess has a four-second timeout and took over 25 seconds on this workstation.


## Triage

### 2026-11-21T01:04:19.788Z — fcs-npc-live-fix

Summary: Replace the slow per-process PowerShell/WMI probe with direct Windows process-handle identity retrieval; validate exact executable/start time and pinned live runtime.
Owner: fcs-npc-live-fix
Priority: high

## Agent suggestion

### 2026-11-21T01:24:44.985Z — fcs-npc-live-fix

Pinned live runtime evidence (2026-11-21): isolated runs 20261121_010727 and 20261121_011815 came online with the recorded EXE identity and DLL generation 417. Later dev-loop stop requests returned stop_partial for all four PIDs because process identity did not match the run manifest; no process was force-terminated. Investigate why start validation accepts these processes but cleanup identity rejects them, then prove start and safe stop against a fresh pinned run.
