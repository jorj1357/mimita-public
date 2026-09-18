# Collision host-contract fix and live touch logging

- EST timestamp: 2026-09-17 16:00:00 -04:00
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Why

The user still fell through all world geometry and reported the JSONL at
`logs/2026-09-18/20260918_012315/events.jsonl`. Reading it (rg / Get-Content)
showed `movement.collision` records with `"result":"solved"` and **no `message`**
and **no `collision.solve.summary` at all**. That EXE
(`mimita-20260917T212310.exe`, 21:23:15) was an intermediate build; the message
payload and package-side records did not exist yet. The log did prove one useful
fact: the package returned `handled=1` (`solved`), so collision "ran" but
produced no contacts.

Root cause found while instrumenting: a **host contract mismatch**. The original
working hot capsule solve received the `GameplayContextV1*` (the context) and
called `ensureWorld` with it. The migration's `collision.main` also treats its
`host` as `GameplayContextV1*`, but `movement.main` was passing `ctx->host` (the
opaque kernel host). The package therefore read world/log capabilities from a
pointer that is not a context, so the world cache and all package logging were
broken. This is the most likely reason nothing collided.

## Changes

- `src/hot-reload/modules/movement-system.cpp`
  - `resolveCollisions` now calls `fn(ctx, &q)`, restoring the original contract:
    the collision package receives the gameplay context. Documented at the call.
  - Collision records now carry actor identity (`actor_id`, `actor_type=player`),
    the frame, and the client/server tick.
- `src/hot-reload/packages/collision/collision-abi.h`
  - `GameCollisionSolveFn` comment now states `host` is the `GameplayContextV1*`.
  - `CollisionSolveV1` gained append-only `frame`, `serverTick`, `clientTick`,
    `actorKind` so records can name the actor and the tick being solved.
- `src/hot-reload/game-api.h`
  - `GameLogEventV1` gained append-only `frame`, `serverTick`, `clientTick`,
    `actorId`, `actorKind`.
- `src/live-code/live-behavior.cpp`
  - `capLogEvent` forwards the new identity/timing fields into `events.jsonl`
    (`actor_id`, `actor_type`, frame/server_tick/client_tick).
- `src/hot-reload/packages/collision/collision-log.h` (rewritten)
  - `collisionLogFull(...)` emits a fully-specified record with actor identity,
    frame, and ticks. `collisionLog`/`collisionLogResult` remain as wrappers.
    Actor kinds: player / npc / remote / other.
- `src/hot-reload/packages/collision/collision-package-solver.cpp`
  - `collision.touch`: emitted immediately on the first solve of an entity and on
    every touch/no-touch verdict change. Shows `touch`, `worldContact`,
    `grounded`, candidate count, contact count, collider count, position, `vz`.
    If this never appears, the solver is not being reached at all.
  - `collision.contact`: one record per accepted contact with the collider/part,
    the **world triangle index** touched, the contact point, normal, penetration,
    and incoming speed. Rate-limited at the source to one per part per tick.
  - `collision.solve.summary`: now includes `worldTris` and is emitted through
    the full-identity path.
  - `collision.declined`: carries actor identity and ticks.
  - `kMaxBodyPush` raised from `0.5` to `1000` so limbs and weapons are
    **authoritative over actor position exactly like the capsule**: an arm or
    weapon sunk into geometry pushes the body out instead of only nudging it.
  - `kLogEveryContact` (live-editable) controls per-contact recording.

## Evidence

- Cold build: `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260917T213605.exe`.
- Hot build: `python devscripts/live-build.py` ->
  `build/hotreload/mimita-live-g000005.dll` (never writes `MiMITA.exe`).
- Runtime: `mimita-20260917T213605.exe --live-code-selftest` ->
  `[LIVE CODE SELFTEST] PASS`, including `log.event capability resolves` and
  `hot capability log reached events.jsonl`.
- The collision DLL contains all new record names:
  `collision.touch`, `collision.contact`, `collision.solve.summary`,
  `collision.declined`, `movement.collision`.
- `git diff --check` passed (exit 0).

## How to read the new stream

```powershell
Get-Content logs\<date>\<hhmmss>\events.jsonl -Wait
rg '"event":"collision.touch"' logs\<date>\<run>\events.jsonl
rg '"event":"collision.contact"' logs\<date>\<run>\events.jsonl
rg '"event":"collision.solve.summary"|"event":"collision.declined"' logs\<date>\<run>\events.jsonl
```

Diagnosis table:

- No `collision.touch` and no `collision.solve.summary` -> `collision.main` is
  never reached (capability or scheduling problem); `movement.collision` with
  `result=no_capability` confirms it.
- `collision.touch` with `touch=0`, `contacts=0`, `candidates>0` -> broadphase
  found geometry but the narrowphase found no touching sphere (tolerance, scale,
  or collider placement bug).
- `collision.touch` with `candidates=0` -> the swept AABB gathered nothing
  (index/region bug).
- `collision.declined` with `cachedTris=0` -> `world.collision` returned no
  geometry.
- `movement.collision` with `parts=0` -> only the capsule resolved; the skeleton
  body-part sockets returned no parts.

## Human verification still required

- Run `mimita-20260917T213605.exe`, fall into the world, and share the new
  `events.jsonl`. `collision.touch` and `collision.solve.summary` will pin the
  exact failing stage.
- Confirm limbs/weapons now stop the player (authoritative body push) and do not
  destabilize walking.
- Set `"collision": "verbose"` to keep records unthrottled if needed.

## Notes

- This was a cold ABI change (`game-api.h`, `live-behavior.cpp`), so the EXE
  rebuild was required and completed.
- `COLLISION` defaults to `off`; records appear at `important` or `verbose`, and
  declines at `errors` or above.
