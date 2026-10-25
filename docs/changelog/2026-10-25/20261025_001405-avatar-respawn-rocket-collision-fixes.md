# Avatar, respawn, rocket cooldown, and coolplace collision fixes

- Time UTC: `2026-10-25T00:14:05Z`
- Display time: `2026-10-24 20:14:05 EDT` (`America/New_York`)
- Branch: `20261011zt`
- HEAD: `0472121b` plus the existing uncommitted worktree
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Fixed four separately reported regressions:

1. Avatar textures stretched after the Juggernaut performance work.
2. Sandbox RESET repeatedly respawning the player.
3. NPC Waves leaving the player dead/floating after an authoritative respawn.
4. Rocket launcher cooldown appearing to wait for unrelated server work.

Also added a coolplace walk-grid scenario and a dev-loop scenario relaunch guard.

## Avatar texture stretching

The performance work in `c1fe8acd` added a combined-body VBO keyed only by the
model path. Avatar atlas application rewrites UVs in place, so an atlas actor
could reuse a buffer built from raw GLB UVs (or the reverse).

- `src/entities/player.h`: added `avatarAtlasUvs`, `renderUploadKey()`, and
  `markAvatarAtlasUvsApplied()`.
- `src/entities/player-render.cpp`: all body-part and merged mesh upload calls
  now pass `renderUploadKey()`.
- `src/entities/player-render-mesh.cpp`: combined-body cache now keys on
  `renderUploadKey()` and reports the upload key.
- `src/avatar/avatar-atlas.cpp`: both atlas application paths mark the player's
  UV state after rewriting UVs.
- `src/entities/player-loader.cpp`: a new model load resets the UV state to raw.
- `src/avatar/avatar.cpp`: the no-custom-model branch now clears
  `gAvatarCanonicalizePlayerModel`.
- `src/avatar/cosmetic-system.cpp`: cosmetic VAO attribute order now matches
  `shaders/basic.vert` (location 1 = UV, location 2 = normal).

Evidence: `logs/10-24-2026/Gameterminal_log_194210.txt:2029` reports
`combined body model=...|authored|atlas parts=6 verts=216` after the clown
avatar load. Before this change the buffer was built before the atlas and was
not rebuilt.

## Respawn and reset

- `src/network/multiplayer-context.h`: adds the explode-request start spawn
  generation and a spawn generation on pending victim health.
- `src/network/multiplayer-packets.cpp`: records the start generation when the
  explode request is sent.
- `src/network/multiplayer-tick.cpp`: clears `awaitingExplodeDeath` when a new
  authoritative spawn generation arrives, not only on a health-0 snapshot.
- `src/engine/engine-tick-net.cpp`: the authoritative spawn packet now clears
  local death, presentation, respawn timer, and pending victim damage, and sets
  the local spawn generation.
- `src/network/multiplayer-projectiles.cpp` and
  `src/network/multiplayer-reconcile.cpp`: pending lethal damage carries the
  life generation and is dropped once a newer generation is known.
- `src/combat/death-system.cpp`: local `DeathSystem` no longer auto-respawns
  while a networked context is active, so the server owns the life.
- `src/network/server-gamemode.cpp`: `serverConsumeNpcWaveLife` is idempotent
  for an actor already marked Dead, so one death costs one life.

Evidence: sandbox reset run `logs/2026-10-24/20261024_234208/events.jsonl`
recorded two spawn publications (initial + reset), one
`client.local.spawn_applied` with `dead_before=true`, and zero explode retries.
NPC Waves run `logs/2026-10-24/20261024_234355/events.jsonl` recorded
`client.local.spawn_applied` with `dead_before=true` after the local death, then
an alive, grounded snapshot.

## Rocket launcher cooldown

- `src/network/weapon-runtime-reconciliation.cpp/.h`: the reconciler accepts the
  result's own server tick and uses the newer of that tick and the latest
  snapshot tick when converting `nextAllowedFireTick` to seconds.
- `src/engine/engine-tick-net.cpp`: passes the attack result server tick.
- `src/combat/weapon-runtime.cpp`: a completed local reload clears the local
  fire cooldown.
- `src/combat/weapon-system.cpp`: the rocket path checks the cooldown even when
  the clip has ammo and the weapon is reloading.

Evidence: run `logs/2026-10-24/20261024_234820/events.jsonl` accepted both
rocket shots. The client log `Gameterminal_log_194821.txt` shows
`[RECONCILE] weapon=rocket_launcher ... cd=0.75s` after the first shot and
`cd=0.73s` after the reload and second shot.

## Coolplace collision

- `src/physics/movement/actor-triangle-solver.cpp`:
  - opposing face seams now use a wider contact distance so separate thin-wall
    triangles are treated as one surface;
  - close opposing connected edges use the same wider distance;
  - the last-valid rollback keeps the strict `0.1 m` threshold when the head or
    torso is penetrating, but allows up to `0.35 m` when only limbs overlap.
    The rollback event now records `core_penetration_m` and the applied
    `fallback_threshold_m`.

Evidence: coolplace run `logs/2026-10-24/20261024_235013/events.jsonl`
reproduced nine `collision.last_valid.rollback` events at the wall near
`(138, -419, 74)` with residual penetrations around `0.10–0.23 m` on
`leftArm`/`leftLeg`. Run `20261024_235447` still reproduced eight rollbacks
after the seam-distance change, which is why the limb-specific rollback
threshold was added. The post-threshold build was compiled and launched, but
the scenario was aborted before a new collision capture; collision feel and
human acceptance remain open.

## Scenario and dev loop

- Added `devscripts/scenarios/coolplace_walk_grid_v1.json`: teleports to the
  three reported positions, walks forward/left/right for 100 fixed ticks,
  waits 60 ticks, and captures a snapshot at each step.
- `devscripts/dev-loop.py`: a scenario now relaunches when the current run has
  no ready client or client PID instead of queuing commands into a dead client.
- Added temporary dev launch modes `16`–`19` for coolplace, NPC Waves, sandbox
  reset, and rocket-set testing.

## Validation

- Canonical dev-loop build: `SUCCESS`, build `3119`, published current.
- Dev-loop launch: mode `16`, run `20261025_001329`, server PID `36188`,
  client PID `27204`, runtime build `3119`.
- No post-threshold collision runtime test, visual acceptance, or human gameplay
  acceptance was performed.

## Pre-existing work preserved

The worktree already contained unrelated edits to configuration, collision
diagnostics, GUI/input, networking, documentation, and the Oct 24 changelogs.
Those were preserved. No commit was created.
