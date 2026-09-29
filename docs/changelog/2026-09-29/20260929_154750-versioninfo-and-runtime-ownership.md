# Versioninfo and Runtime Ownership Diagnostics

Time: `2026-09-29T15:47:50Z`

## Request

Add the v8-style `versioninfo` command to v9 so the active executable,
process, ticks, and exact `events.jsonl` destination can be identified while
investigating collision behavior.

## Change

Added the v9 `versioninfo` terminal command. It prints and logs:

- process role, PID, run ID, and uptime;
- exact running executable path;
- exact structured-log `events.jsonl` path;
- client and latest received server ticks;
- hot DLL loaded state and reload generation;
- room and server identity.

It emits `versioninfo.executed` into the same JSONL stream.

The source review also confirmed that v9 still contains a guarded legacy GLB
collision pipeline beside the active player actor-triangle solver. The config
currently enables the triangle solver for local players, while NPCs and the
fallback path still retain older capsule/body/safety phases.

## Evidence

- Build `0403` completed with `Status: SUCCESS`.
- Build `0403 --collision-selftest` returned `[COLLISION SELFTEST] PASS`.
- The binary contains `versioninfo`, `versioninfo.executed`, and the JSONL path
  output strings.
- The previously supplied run `20260929_112825` is a server-only JSONL stream;
  its paired client run `20260929_112421` contains no bookmark events.
- Live `versioninfo` output and a successful bookmark press remain pending.

