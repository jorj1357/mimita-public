# Incomplete latest artifact prevented dev-loop startup

- First observed: 2026-11-06T01:33:29Z (2026-11-05 20:33:29 EST)
- Status: FIXED — daemon recovery and latest-artifact launch verified; visual review pending
- Related implementation: `devscripts/dev-loop.py`, `publish_build()` and `DevLoop.__init__()`
- Related run evidence: `logs/2026-11-06/20261106_015800/`
- Related fix record: [20261106_021300-artifact-pointer-recovery.md](../../changelog/2026-11-06/20261106_021300-artifact-pointer-recovery.md)

## Expected behavior

The dev-loop daemon should start when any older published artifact is valid. A
failed build must not prevent an explicit launch of the last valid artifact.
The specification says: “If no successful artifact exists yet, `[1]` must
queue or join the one active build and launch the first valid successful
artifact as soon as publication completes. If the build fails, report the
failure and do not claim that a game was launched.” It also requires the last
successfully published artifact to remain available after a failed build.

## Actual behavior

The latest build attempt reported `Nothing changed`, then failed publication
because the source executable had an invalid DOS header or was too small. Its
timestamped destination directory remained under `.dev/builds/` without
`build-identity.json`. Startup chose that directory merely because its name
sorted newest, then read the missing sidecar without handling `FileNotFoundError`.
The daemon exited before opening its control endpoint. The watchdog repeatedly
restarted it, and the dashboard could not process **Launch latest stable
MiMITA.exe**.

The earlier launch request in run `20261106_012938` did start a server and
client from a valid older artifact, and the client connected. That request
later failed the mode-51 “active” readiness check; two subsequent explicit
CLI stop requests ended that run. Those events explain why an earlier attempt
also appeared unsuccessful, but they are separate from the daemon startup
failure.

## First incorrect step and cause

`publish_build()` created the final artifact directory before validating and
copying the linker output. If copy/validation failed, the exception left a
partial directory that looked like a candidate to `newest_published_build()`.
`DevLoop.__init__()` then trusted that directory and attempted to read its
missing identity sidecar. The unchecked directory selection was the first
incorrect step in the button-to-process path: the button itself correctly sent
a launch control request, but no daemon existed to receive it.

## Fix

- `publish_build()` now removes the incomplete destination on any publication
  failure and refuses to write identity metadata for an invalid executable.
- `DevLoop.__init__()` now restores the newest fully verified published
  artifact, skipping incomplete folders and preserving older valid builds.

## Evidence

- Before the fix, `.dev/builds/2026-11-06T01-33-12.179985Z/build-identity.json`
  was absent, and the watchdog log repeatedly recorded a `FileNotFoundError`
  from `DevLoop.__init__()` while trying to start the daemon.
- After the fix, the daemon started while that incomplete folder remained
  present, selected the prior valid artifact, and completed a real launch.
- The live dev-loop then completed an incremental build and published valid
  artifact `2026-11-06T01:57:33.707752Z` with SHA-256
  `127ddfac0f072a54d1533a84a6a1eda6ab45e97ff9b42f9d0eba20c5d19e5f00`.
- A second explicit launch selected that newest artifact. Run
  `20261106_015800` reports `status=running`, server PID `26064`, client PID
  `16740`, `server_ready=true`, `client_ready=true`, and `builds_match=true`.
  The fixed `.dev/current/mimita.exe` exists and both processes use it.
- The dashboard button was then pressed directly. Its control request
  `dashboard-15c6aada` (`build-or-launch`) completed. Current run
  `20261106_020329` reports `server_ready=true`, `client_ready=true`, and
  `builds_match=true`; server PID `8812` and client PID `2476` both run
  `.dev/current/mimita.exe`. This confirms the button path itself works after
  daemon recovery.
- The artifact metadata reports `source_changed_during_build=true`; the
  source was dirty and its fingerprint changed during that build. Runtime
  identity/readiness are verified, while visual/gameplay acceptance of this
  build remains open. At the latest check a newer source generation was still
  building; the running pair remained healthy on the last verified artifact
  with auto-restart disabled.
