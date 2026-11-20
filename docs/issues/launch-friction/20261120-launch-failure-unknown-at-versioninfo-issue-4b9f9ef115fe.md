# issue-4b9f9ef115fe: Launch failure: unknown at versioninfo

## Human report (verbatim, immutable)

> versioninfo failed: Command '['C:\\mimita-v9\\.dev\\current\\mimita.exe', '--versioninfo']' timed out after 20 seconds


## Triage

### 2026-11-20T07:31:06.890Z — launch-latest-stable-fix

Summary: Repeated launch preflight timed out because --versioninfo initializes the full structured logger before a server can start; remove the scan from this lightweight identity probe and prove the exact launch path on the current build.
Owner: launch-latest-stable-fix
Priority: high
