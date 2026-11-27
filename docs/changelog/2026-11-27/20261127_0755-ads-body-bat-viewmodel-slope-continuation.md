# ADS body gesture, bat viewmodel, and walkable-slope wiring

Session: `opencode-deepseek-v4-3`, continuation of
`docs/handoffs/2026-11-27-body-rig-ads-damage-handoff.md`.

## What changed

- ADS body gesture (item 2, partial):
  - `src/combat/weapon-config.h/.cpp` parse `ads.blend_seconds`.
  - `src/combat/weapon-viewmodel.cpp` derives the effective ADS rate as
    `3 / blend_seconds` and reports both configured and effective rates.
  - `config/weapons/bloodtest_ak47/config.json` and
    `config/weapons/bloodtest_judgement/config.json` set `blend_seconds: 0.33`.
  - `src/entities/player-animation.cpp` adds a per-Player ADS gesture blend
    (bounded static map), torso yaw/pitch, head tilt, arm aim, and the
    `weapon.ads.pose` event with blend, sight alignment, hand-to-weapon
    distances, torso/head angles, and blend timing.
- Bat first-person viewmodel (item 5, complete):
  - Root cause: `bloodtest_bat` and `captain_falcon_moveset` both declared
    slot 30. `getDefForSlot(30)` selected `captain_falcon_moveset`, whose
    viewmodel is disabled and model path is empty.
  - `config/weapons/captain_falcon_moveset/config.json` slot moved to 31;
    `bloodtest_bat` remains slot 30.
  - `src/combat/weapon-viewmodel.cpp` emits `weapon.viewmodel.state` with
    requested/loaded path, slot, equipped, loaded, verts, vao, bounds, and
    visible.
- Walkable-slope config (item 7, partial):
  - `src/config/collision-config.cpp` clamps `walkableSlopeDot` 0..1, assigns
    `MAX_WALKABLE_SLOPE_DOT`, and emits `collision.slope.config`.
  - `src/config/movement-config.cpp` defaults `walkableSlopeDot = 0` ("follow
    the shared global"); `src/physics/movement/movement-step.cpp` falls back
    to `MAX_WALKABLE_SLOPE_DOT`.
- Gen-785 startup regression fix:
  - The first item-7 implementation called `MovementJsonConfig::instance()`
    from `CollisionConfig::load`; the movement default builder calls
    `CollisionConfig::instance()`, causing recursive static initialization.
  - Gen 785 server processes exited during config load. The final code removes
    that call and keeps one shared threshold.

## Build evidence

- Gen 784: `game-dll-20261127T072316157011Z-06c929`, success, 68.594 s,
  10 compiled / 637 skipped. Postmortem: mean host CPU 97.47%, peak 100%,
  minimum free memory 23 MB; suggestion to move concurrent tests off the
  critical path.
- Gen 785: `game-dll-20261127T073731029285Z-efcc7c`, success, 40.552 s.
  Runtime-broken: two isolated servers exited before publishing a port.
- Gen 786: published 2026-11-27T07:45:59Z from
  `build-request-2792da10f482`; server and client started cleanly at
  `20261127_074621`. Gen 786 also included other agents' saved sources
  (45 translated units).
- This is build evidence only, never behavior proof.

## Runtime evidence

- ADS, run `20261127_072858`, gen 784,
  `logs/2026-11-27/20261127_072858/client-01/events.jsonl`:
  `weapon.ads.pose` for `ADS Dev` with `ads_requested=true`, blend
  `0.99985`/`0.9999998`, `blend_seconds=0.3300000131`,
  `blend_rate=9.0909090`, `torso_pitch_deg=-2.3582`, `torso_yaw_deg=0.0`,
  `head_tilt_deg=-7.9988`, `bodycam_smoothing=true`; matching
  `weapon.ads.state` shows configured 18.0, effective 9.0909, 0.33 s.
  Residual: `right_hand_grip_m` and `left_hand_foregrip_m` were 0.46-6.6 m and
  `sight_alignment_m` 2.4-7.3 m, so two-hand lock is **not** proven.
- Bat, run `20261127_074621`, gen 786,
  `logs/2026-11-27/20261127_074621/client-01/events.jsonl`:
  after `agent.input equip 3` with weapon set 33,
  `weapon.viewmodel.state` reported `weapon=bloodtest_bat`,
  `requested_path=loaded_path=assets/objects/weapons/mimita-bat-v1.glb`,
  `loaded=true`, `verts=222`, `vao=9`, `has_bounds=true`,
  `equipped=true`, `equipped_slot=30`, `visible=true`,
  `reason=held-visible`. Human visual acceptance of the on-screen bat is still
  separate.
- Slope, same run: `collision.slope.config` at startup reported 0.8
  (`changed=false`); hot-editing to 0.85 produced `changed=true`,
  applied `0.8500000238`; restoring 0.80 produced `changed=true`, applied 0.8.
  `collision.slope.classify` and a real ramp traversal remain open.
- Gen-785 startup fix: run `20261127_074621` server journal has
  `server.process.started`; the isolated run reached `online`.

## Proof records

- `task-proof-c1925804fc01` `bat-viewmodel` proven.
- `task-proof-5debe11dd6c1` `slope-config-wiring` proven (config only).
- `task-proof-6d8f90953782` `gen785-startup-fix` proven.
- `task-proof-f8af8bdcbe91` `ads-body-pose` proven for the event contract;
  issue `issue-9c05cbcd5b0d` remains open for arm lock.
- Issue `issue-f5430e84b28b` closed (bat). Issue `issue-9c865462d90d` remains
  open (classify/ramp). Duplicate `issue-fbea4c33bc56` closed as duplicate of
  `issue-9c05cbcd5b0d`.

## Open items

- Item 2: real two-hand lock to grip/foregrip and a trustworthy alignment
  measurement; current gesture blend is proven.
- Item 3: fullscreen-6 all-UI stretched; `hud-fs6-crosshair` files remain
  owned by that agent.
- Item 4: damage mode 1/2 and authoritative networking; not started.
- Item 6: NPC ranged shooting gate; no `npc.fire` events were observed in the
  TDM proof run; not investigated further.
- Item 7: `collision.slope.classify` and ramp traversal proof.
- Gen 785 is runtime-broken; use gen 786 or later. Gen 784 remains the
  last known-good pre-fix server generation.

## Files

- `src/combat/weapon-config.h`
- `src/combat/weapon-config.cpp`
- `src/combat/weapon-viewmodel.cpp`
- `src/entities/player-animation.cpp`
- `src/config/collision-config.cpp`
- `src/config/movement-config.cpp`
- `src/physics/movement/movement-step.cpp`
- `config/weapons/bloodtest_ak47/config.json`
- `config/weapons/bloodtest_judgement/config.json`
- `config/weapons/captain_falcon_moveset/config.json`
- `config/collision.json` (value restored to 0.80 after proof)
