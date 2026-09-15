# Movement policy verified already hot/shared (no duplicate migration)

Date: 2026-09-15 21:00 EST (UTC 2026-09-16T01:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (verification only; no code changes)

## What was requested

Migrate dash/down-dash, then freeze, then post-acceleration speed clamp, then
generic integrator authority, as shared hot movement policy.

## What was found

All of it is **already implemented by the concurrent movement agent** as shared
hot policies:

- `src/hot-reload/hot-movement-policy.h`: `GameDashPolicyV1`,
  `GameFreezePolicyV1`, `GameSpeedClampV1` (+ air/ground/gravity/speed/jump).
- `src/hot-reload/modules/movement-dash.cpp`, `movement-freeze.cpp`,
  `movement-speed-clamp.cpp`, `movement-system.cpp`: the single hot
  implementations.
- Cold server movement (`src/physics/movement/movement-step.cpp`) dispatches
  `GAME_EVENT_MOVEMENT_DASH`/`FREEZE`/`SPEED_CLAMP` via
  `LiveBehavior::dispatchGameplayEvent64`; local prediction calls the same
  `MimitaHotMovement::*` functions. No duplicated formula.

## Why no changes were made

Re-implementing would create a second, duplicate movement owner and conflict with
the concurrent agent's ownership (explicit concurrency boundary: server movement
/ MovementRuntimeState / movement-system architecture belong to the movement
agent). The architecture-first rule is to reach one owner, so verifying the
hot owner already exists is the correct outcome.

## Evidence

- `mimita.exe --movement-algorithm-selftest` -> PASS, incl. "hot dash owns the
  grounded dash", "hot dash owns the airborne dash", "hot dash denies when
  unavailable", "hot dash uses camera fallback direction", "hot dash owns the
  down-dash vertical response", "hot freeze owns activation and velocity
  suppression", "hot freeze keeps the actor frozen while held", "hot freeze owns
  release/exit", "hot freeze clamps timer to the max duration", "hot speed clamp
  owns horizontal max-speed enforcement", "hot speed clamp preserves velocity
  when disabled".
- `mimita.exe --movement-parity-selftest` -> PASS.

## Report

- SUBSYSTEM: movement policy (air/ground/gravity/speed/jump/dash/down-dash/
  freeze/speed-clamp).
- COLD OWNER REMOVED: none by this agent (already removed by the movement agent).
- HOT OWNER ADDED: none by this agent (already present).
- COMPATIBILITY FALLBACK: cold built-in formula remains the fallback when no hot
  handler is active.
- STATE AUTHORITY: `MovementRuntimeStateComponent` (generic), filled by cold,
  mutated by hot.
- SERVER CALL SITE: `physics/movement/movement-step.cpp` via generic events.
- CLIENT CALL SITE: `hot-reload/modules/movement-system.cpp` (`movement.main`).
- SELFTEST PROVEN: movement algorithm + parity selftests PASS.
- PARITY STATUS: parity selftest PASS (no divergence reported by the harness).
- LIVE HOT-EDIT PROVEN: no.
- KNOWN BUGS DEFERRED: movement feel/tuning.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? For dash/down-dash/freeze/speed
  clamp policy: no (already hot). Integrator authority / remaining movement
  mechanism: movement agent's area.
- NEXT COLD OWNER (this agent's area): generic surface-effect/decal primitive,
  then muzzle flash, camera/screen policy, remaining audio policy, weapon
  presentation.

## Files changed

`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
