# Live collision diagnostics in events.jsonl

- EST timestamp: 2026-09-17 15:00:00 -04:00
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Make the collision "no collisions / fall through the world" failure observable
live while `MiMITA.exe` runs, using the new single JSONL debug stream
(`debug::logEvent` / `events.jsonl`). No more guessing: the stream must show
which collision branch ran, whether the world cache was available, how many
candidates/contacts were found, and whether the actor grounded or bounced.

## Changes

- `src/hot-reload/packages/collision/collision-log.h` (new)
  - Hot-side bridge from the collision package to the kernel's `log.event`
    capability. Plain data only; the kernel owns `events.jsonl`. Resolves the
    capability per call and no-ops when unavailable, so the package never
    depends on an EXE symbol directly.
- `src/hot-reload/packages/collision/collision-package-solver.cpp`
  - `logDecline(...)`: throttled (1/s) `COLLISION` / `collision.declined` record
    carrying `why`, cached triangle count, `ready`, position, and `vz`, plus the
    number of suppressed repeats. Emitted for `world_unavailable` and
    `non_finite_input` - the exact "no collision at all" modes. A decline is
    Error level so the kernel never aggregates it away.
  - Per-second `COLLISION` / `collision.solve.summary` record:
    `solves`, `worldTris`, broad/narrow/total ms, `avgCand`, `large`,
    `avgContacts`, `noContact`, `grounded`. This proves the broadphase actually
    gathered geometry and produced contacts.
  - `COLLISION` / `collision.impact` record on any tick with an accepted impact:
    part id, world point, normal, contact count, grounded/bounced.
- `src/hot-reload/modules/movement-system.cpp`
  - `logMovementBranch(...)`: throttled (1/s) `COLLISION` / `movement.collision`
    record naming the branch that did NOT solve (`no_capability`, `declined`),
    with entity, position, and `vz`.
  - A throttled solved record naming `branch=solved`, collider count, resolved
    body-part count, `grounded`, `worldContact`, position, and `vz`. This is the
    one line that says whether the package saw the capsule *and* the limbs.
- `src/live-code/live-code-selftest.cpp`
  - New self-test: resolve `log.event` through the real gameplay host context,
    emit a record, and assert it appears in `events.jsonl`. This proves the same
    path the collision package uses, independent of graphics.

## Evidence

- Cold build: `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260917T212148.exe`.
- Hot build: `python devscripts/live-build.py` ->
  `build/hotreload/mimita-live-g000004.dll` (never writes `MiMITA.exe`).
- Runtime: `mimita-20260917T212148.exe --live-code-selftest` ->
  `[LIVE CODE SELFTEST] PASS`, including
  `[ok] log.event capability resolves` and
  `[ok] hot capability log reached events.jsonl`.
- Proof record in `logs/2026-09-18/20260918_012159/events.jsonl`:
  `{"level":"INFO","category":"NETWORK","event":"selftest.capability_log","result":"ok"}`
  emitted through the same capability the collision package calls.
- `collision.main` still resolves in the package summary (`providers=9`).
- `git diff --check` passed (exit 0).

## How to watch it live

```powershell
Get-Content logs\<yyyy-mm-dd>\<hhmmss>\events.jsonl -Wait
rg '"category":"COLLISION"' logs\<date>\<run>\events.jsonl
rg '"event":"collision.declined"|"event":"movement.collision"|"event":"collision.solve.summary"' logs\<date>\<run>\events.jsonl
```

Interpretation:

- `movement.collision` with `branch=no_capability` -> the package capability did
  not resolve; nothing owns collision.
- `collision.declined` with `why=world_unavailable` and `cachedTris=0` -> the
  `world.collision` capability returned no geometry; the actor is intentionally
  not integrated against a missing world.
- `collision.solve.summary` with `avgCand=0` -> geometry exists but the swept
  AABB gathered nothing (index/region bug).
- `collision.solve.summary` with `avgContacts=0` and `noContact` climbing ->
  candidates exist but the narrowphase found no touching sphere (tolerance/scale
  bug).
- `movement.collision` with `branch=solved` and `parts=0` -> only the capsule
  resolved; the skeleton body-part sockets did not return parts.

## Human verification still required

- Run the game and confirm the JSONL updates live during play (walk, fall, dash,
  wall contact) with the commands above.
- Set `"collision": "verbose"` in `config/debuglogger.json` to see per-tick
  collision records if the throttled summaries are too coarse.

## Notes / follow-ups

- The collision package's own headless self-test calls `collisionSolve(nullptr,
  ...)`, so it cannot emit capability logs; that is why the round-trip proof was
  added at the live-code self-test level with a real host context.
- `COLLISION` defaults to `off` in `config/debuglogger.json`, so collision
  records are gated off until the category is enabled. Set it to `important` or
  `verbose` to observe them; the decline path is Error level and is written when
  the category is at least `errors`.
