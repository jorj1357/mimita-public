# Payload on Mimita Pipes join crash investigation

Date: 2026-11-28

## Report and issue

Preserved the user's report in issue [issue-c9b65fb9948e](../../issues/bug/20261128-diagnose-payload-join-crash-on-mimita-pipes-with-issue-c9b65fb9948e.md): investigate a crash joining a Payload server on Mimita Pipes, make false conditions unmistakably obvious, and suggest the diagnostic approach.

## Evidence

- `evidence-watch.py list --json` found server PID 17456 running `.dev/current/mimita.exe` with `--map mimita-pipes --mode payload --npcs 1`, started at `2026-11-28T06:09:03.597Z`.
- The process is `source=unverified`, with empty run/build identity and no discoverable journal. It was read-only inspected and left untouched.
- `config/maps/mimita-pipes.json` is static and has 20 enabled Payload route points ordered 0 through 19. Inspection did not establish a malformed or empty route.
- `src/network/server-packets.cpp::logJoinStage` (lines 63-129) already logs failed/rejected join stages through StructuredLogger, including stage, result, reason, map/mode, PID, and a correlated network event for rejection. This makes a generic server-side join rejection less likely to be silent; it does not rule out client-side failure, process crash, lost logs, or later map/spawn/snapshot stages.
- Existing broad lifecycle issue `issue-55bd775c8026` is related. The new map-specific issue keeps this symptom independently traceable and includes the concrete proposed investigation and diagnostic contract.

## Conclusion and next step

Root cause is undetermined: the observed matching server was alive, but the report does not identify which process crashed and there is no run journal or crash dump tied to it. Reproduce with the root dev-loop, pin the exact build and both process identities, trace the correlated client/server join path through map readiness, spawn, and first snapshot, then inspect the first missing stage and process exit/dump. Add owner-level diagnostics only for a false condition found on that path: one structured ERROR at the transition, naming the predicate and expected/actual values, function/source, actionable reason, and run/build/join IDs; preserve recent events on fatal exit and summarize repeats without per-tick spam.

No source change, build, or runtime proof was made. Runtime process 17456 was not controlled.
