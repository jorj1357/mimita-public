# Ragdoll toggle policy moved behind hot capability

- Timestamp: 2026-09-24 18:30:00 EST
- Reference behavior: `afad20a`
- Focused skill: `docs/skills/spec-behavior-review-v1.md`

## Implementation

- Added versioned `GAME_CAP_RAGDOLL_TOGGLE` and `GameRagdollToggleV1` to the
  append-only hot ABI in `src/hot-reload/game-api.h`.
- Added the stable kernel bridge `capRagdollToggle` in
  `src/live-code/live-behavior.cpp`. It is mechanism-only: it applies a hot
  request to the persistent local `Player` and `RagdollModeSystem` state.
- Added `ragdoll.toggle-policy` to the replaceable DLL in
  `src/hot-reload/modules/ragdoll-solve.cpp`. The DLL reads the current G
  input, owns the fresh-edge policy, and calls the generic capability.
- Removed the old feature-specific toggle decision and edge state from
  `src/sim/simulate-tick.cpp`.

After one compatible EXE installation, editing ragdoll toggle policy, solver,
aim, presentation, or JSON tuning does not require another cold ragdoll build.
The EXE bridge is stable runtime mechanism, not ragdoll policy.

## Validation

- Hot generation `build/hotreload/p7832/gen7/mimita-live-g000007.dll` built
  successfully and includes the changed API and ragdoll sources.
- `git diff --check` passed; only line-ending warnings were reported.
- Cold installation remains blocked by pre-existing unrelated errors in
  `src/network/server-npcs.cpp` (`MimitaNet::GameNpcLifecycleFn` and
  `SpawnPoint::yaw`).
- No running EXE was closed, killed, replaced, or unlocked.
- Live G-toggle and visual afad20a parity still require manual acceptance after
  a compatible EXE containing the new kernel capability is installed.
