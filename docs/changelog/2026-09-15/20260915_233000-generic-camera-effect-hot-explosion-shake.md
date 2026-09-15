# Generic camera-effect primitive + hot explosion camera shake

Date: 2026-09-15 23:30 EST (UTC 2026-09-16T03:30:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## SUBSYSTEM
Client presentation - camera effects (explosion shake).

## GENERIC PRIMITIVE ADDED
`GAME_CAP_CAMERA_EFFECT` (`camera.effect`) + `GameCameraEffectV1` (pitch/yaw
amplitude, falloffDistance/distance, sourceEntity, runtimeKey). Kernel
`capCameraEffect` applies `camera.addPunch` with distance attenuation and has no
feature branch. `EffectRequestV1` gained generic `distance`/`falloffDistance`
fields.

## REAL SHIPPING PATH MIGRATED
Explosion camera shake in `weapon-rocket-launcher.cpp` (real local rocket
explosion): it now emits a generic `effect.camera.shake` fact; the hot
`hot.effect-composition` policy chooses amplitude and falloff and emits the
generic `camera.effect` command. The cold `camera.addPunch` runs only when no hot
handler handles the fact (compatibility fallback).

## COLD OWNER REMOVED
Explosion-shake amplitude/falloff decision leaves `weapon-rocket-launcher.cpp`.

## HOT OWNER ADDED
`effect.camera.shake` branch in `modules/presentation/effect-composition.cpp`;
`hotcamerafx` command.

## COLD MECHANISM REMAINING
`Camera::addPunch` / camera transform application (legitimate).

## COMPATIBILITY FALLBACK
Cold `camera.addPunch` when no hot handler handles the fact.

## RUNTIME-UNKNOWN PROOF
`hotcamerafx` requests a camera effect the EXE never defined a concept for - no
enum, no feature switch.

## SELFTEST PROVEN
"real explosion shake reaches the hot camera policy"; "hot camera-effect command
works (no enum)"; full suite PASS. `build_agent.py` -> SUCCESS.

## LIVE HOT-EDIT PROVEN
No (no screen).

## CONCURRENCY BOUNDARY STATUS
Clean (no movement/network files touched). Transient blocker: the concurrent
agent's in-progress `reconcile-policy.cpp`/`hot-reconciliation.h` broke the DLL
build (their area, since fixed); a stale `live-behavior.o` was invalidated to
force recompile.

## BUGS DEFERRED
Screen flash / damage vignette; shake feel; weapon-fire/footstep audio; weapon
presentation; tuning.

## WOULD THIS BUG STILL REQUIRE COLD RESTART?
Camera shake policy: **no** (hot). Screen-flash/damage-vignette policy: yes until
migrated. Camera matrix/backend bugs stay cold.

## NEXT COLD OWNER
Generic screen-effect primitive (or reuse hot UI) + one real screen path; then
weapon-fire audio; footstep audio; weapon presentation.

## Files changed
`src/hot-reload/game-api.h`, `src/live-code/live-behavior.{h,cpp}`,
`src/hot-reload/modules/presentation/effect-composition.cpp`,
`src/combat/weapon-rocket-launcher.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
