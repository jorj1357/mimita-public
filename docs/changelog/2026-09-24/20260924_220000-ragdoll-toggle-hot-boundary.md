# Ragdoll toggle diagnosis and hot-boundary follow-up

- Timestamp: 2026-09-24 18:00:00 EST
- Branch: current working branch
- Reference: `afad20a`
- Focused skill: `docs/skills/spec-behavior-review-v1.md`

## Change

`src/sim/simulate-tick.cpp` changed the local ragdoll toggle path so a fresh
`G` edge is always evaluated. The previous code silently skipped the entire
toggle when `RagdollModeConfig::data().enabled` was false. The new path checks
the flag after the edge, logs an explicit ignored reason, and sets
`Player::ragdollModeActive` from `RagdollModeSystem::isActive()` after
activation instead of assuming activation succeeded. Toggle activation and
deactivation now emit categorized ragdoll diagnostics with the simulation tick.

## Hot/cold result

`src/hot-reload/modules/ragdoll-solve.cpp` and
`src/hot-reload/modules/ragdoll-aim.cpp` remain replaceable and the hot DLL
compiled successfully. The toggle bridge remains cold because it directly owns
`Player` and `RagdollModeSystem` lifecycle state in the fixed simulation host.
Moving that lifecycle operation fully hot requires a versioned command/event or
capability bridge; this session did not invent a second state owner.

## Validation

- `git diff --check`: passed before the edit.
- Hot DLL compilation: passed (`[HOT RELOAD] DLL build success`).
- Cold build: blocked by unrelated existing errors in
  `src/network/server-npcs.cpp`: missing `MimitaNet::GameNpcLifecycleFn` and
  missing `SpawnPoint::yaw`.
- No running executable was closed, killed, replaced, or unlocked.
- Runtime G-toggle and visual afad20a parity remain unverified until a fixed
  executable containing this bridge can be launched and manually tested.

## Pre-existing edits

All unrelated working-tree modifications were preserved.
