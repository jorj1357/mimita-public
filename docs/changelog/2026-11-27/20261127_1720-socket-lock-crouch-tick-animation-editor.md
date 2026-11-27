# Socket lock, global crouch, tick-advanced NPC animation, visible=hit spec, editor scaffold

- Task/agent: `anim-socket-crouch`
- Date, time, timezone: 2026-11-27T17:20Z, ISO 8601
- Status: core runtime-proven; visible=hit limb capsules and full editor GUI remain open
- Linked issue: `issue-2d4032ac8044`

## Requested behavior

- All six body parts (torso/head/arms/legs) tethered to their torso socket by
  `0.01 m`, JSON-configurable.
- Crouch works in every mode through the shared crouch action, with a `0.5x`
  stand-height capsule anchored at the feet and a configurable leg socket lift.
- Remote/NPC animation advances by authoritative server ticks, not render
  frames, so clips do not stick at the first frame.
- Damage/hit capsules use the visible limb pose (`what you see = what you hit`)
  for damage/hit only; no standalone invisible limb box.
- Cosmetics and weapon hand attachment use the same render-pose function as the
  body.
- First animation/weapon pose editor surface in the map editor: numbered
  `anim_list`, `anim_play`, `anim_stop`, `anim_scrub`, `anim_mode` + G/R/S, with
  a spec for the GUI and override saves.

## Implementation changes

- `config/animations/settings.json`: `rootTranslationMode: 2`; all six
  `rootTranslationLimits` at `0.01`; added `animationAdvanceMode: 2`,
  `crouchHeightScale: 0.5`, `crouchLegSocketLiftM: 0.35`,
  `crouchSocketBlendSeconds: 0.12`.
- `src/entities/player-avatar-animations.{h,cpp}`: parse/validate/log the new
  settings; default mode 2.
- `src/entities/player-animation.cpp`: smoothed crouch leg-socket lift applied
  to the leg rest socket before `restM * poseM`.
- `src/entities/player.cpp`: `getCapsule()`/`getOBB()` shrink height by the
  crouch scale while keeping the bottom on the ground.
- `src/sim/simulate-tick.cpp`: sets `player.crouching` from the shared input in
  every mode, emits `crouch.capsule`, and passes crouch height scale to the
  actor-pair capsule.
- `src/physics/movement/actor-pair-collision.{h,cpp}`: optional capsule height
  scale for crouch, radius unchanged.
- `src/network/multiplayer-interpolation.cpp`: remote animation advances by
  `(serverTick - lastAnimationServerTick) * fixedDt`, clamped to 6 ticks, when
  `animationAdvanceMode == 2`.
- `src/network/server-projectiles.cpp`: server player projectile capsule uses
  the replicated crouch state and the same height scale.
- `src/avatar/cosmetic-system.cpp` and `src/combat/weapon-viewmodel.cpp`:
  cosmetic and weapon-hand attachment use `playerRenderPartTransform()`.
- `src/entities/aim-commands.cpp`: `anim_list`, `anim_play`, `anim_stop`,
  `anim_scrub`, `anim_mode` backend commands and `anim.editor` events.

## Specifications

- `docs/specs/animations/animations.md`: updated runtime settings, NPC tick
  advance, crouch/capsule policy, and a new “Visual and hit-pose parity”
  section (damage/hit first; per-limb world collision deferred).
- New `docs/specs/animations/animation-editor.md`: map-editor flow, numbered
  clip list, loop/stop/scrub, G/R/S, override saves, weapon pose layer, IK and
  Blender import/export as later phases.

## Issues filed

- `issue-2d4032ac8044` main request.
- `issue-48c90c86187d` white gore debris blocks.
- `issue-e873cbbdbeeb` teammate healthbar / no enemy healthbar.
- `issue-d7ea7a00420f` ICE send errors, ghost hits, rejoin timeout + auto-flag.
- `issue-0c39edc4f9b8` freecam stale position/backlog.
- `issue-8d6db25817ec` monster/big-avatar limb collision capsules.
- `issue-f37e0198bbf1` in-engine GLB/OBJ vertex/UV/texture editing.

## Evidence and remaining work

- Build generation 801 published (`game-g801.dll`, sha256
  `c3d0af077262851b9fae80387a39858e1e2c05db8337433fd0a96e532d4add33`).
- Run `20261127_180833` (Bloodtest, client-01, generation 801):
  - `anim.settings.loaded`: `root_translation_mode=2`,
    `animation_advance_mode=2`, `crouch_height_scale=0.5`,
    `crouch_leg_socket_lift_m=0.35`, all tether budgets `0.01`.
  - `anim.tether.verdict`: `matches=true`, `max_out_len=0.00999977`,
    `over_budget_count=0`.
  - `crouch.capsule`: crouched height `1.80002` (0.5 x 3.6), bottom Z unchanged
    (`731.316` standing vs `731.316` crouched).
  - `jitter.verdict`: `matches=true`, actor `accel_max_m=0.000983`,
    `raw_lag_max_m=0.000341`, 2028 samples.
  - `anim.editor` play/stop delivered (`anim_play 3`, `anim_stop`).
- Run `20261127_181532` (Bloodtest with NPC, generation 801):
  `anim.npc.ticklocked` clip_tick advances instead of sticking at 1.0
  (walk 6→22; sprint 2→12→31→47; 131 monotonic steps of 156 samples).
- Not yet claimed: per-limb damage/hit capsules for ordinary players (spec and
  existing part-box hitscan trace only), full map-editor GUI/timeline/gizmos
  (terminal backend exists), weapon-pose editor, state-dwell hysteresis, and
  server crouch capsule outside projectile queries.
