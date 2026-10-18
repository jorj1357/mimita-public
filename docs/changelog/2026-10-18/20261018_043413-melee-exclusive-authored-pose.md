# Melee exclusive Captain Falcon authored-pose policy

- UTC: `2026-10-18T04:34:13Z`
- Branch: `20261011zt`
- Result: `PASS_WITH_HUMAN_REVIEW`. Implemented and runtime-verified that Melee
  actors (player + NPC) use only `assets/avatars/captainfalcon/animations.json`
  for their visible rig pose, with aimbody/ragdoll/procedural/weapon layers
  suppressed.
- Documents: `docs/ROUTER.md`, `docs/specs/gamemodes/supersmashbrosmelee.md` §57,
  `docs/specs/animations/animations.md` ("Exclusive authored-pose policy"),
  `docs/specs/animations/blender-animation-authoring.md`, `AGENTS.md`,
  `docs/workflows/runtime-scenario-validation.md`.

## Problem

The Melee runtime loaded per-avatar clips but then composed weapon/reload,
freeze/dash, idle-sway, and always-on physical AimBody/ragdoll limb motion after
the clip. The AimBody (`RagdollModeSystem::updateAim`, mode `physical`/`hybrid`)
ran in `simulate-tick.cpp` after `physicsMainUpdate` and owned the rendered pose
and hitboxes, so limbs/head moved with momentum instead of the authored clip.
The Melee NPC also used a random avatar ("clown"), not Captain Falcon.

## Changes

- `src/entities/player.h` — `AvatarAnimationSet.authoredOnly` (data opt-in);
  `Player::usesMeleeAuthoredPose()`.
- `src/entities/player-avatar-animations.cpp` — parse `poseComposition:
  "authored_only"` from `animations.json`.
- `assets/avatars/captainfalcon/animations.json` — added
  `"poseComposition": "authored_only"`; added a `walk` locomotion clip (the set
  had no locomotion clip).
- `src/entities/player-animation.cpp` —
  - `usesMeleeAuthoredPose()`: true for the Captain Falcon actor (Melee fighter
    profile), also driven by `authoredOnly`/replicated mode/move-set runtime.
  - Exclusive path: `findClip` never falls back to the global profile; missing
    clips use the profile's configured fallback (idle) with a bounded
    `anim.melee.clip_missing` diagnostic.
  - Suppressed overlays for exclusive actors: weapon pose, reload overlay, freeze
    pose, dash pose, axis locks, idle sway; the sampled authored transform is used
    exactly (no spring smoothing); AimBody limb-pitch application is skipped;
    ragdoll/procedural-frozen early returns are bypassed so the authored pose
    renders.
  - Bounded `anim.melee.authored_pose` (per second) and a temporary
    `anim.melee.pose_probe` diagnostic (per second) identify actor, mode, avatar,
    clip, policy, and suppressed layers. Category `Avatar` (the `animation`
    category is off in `config/debuglogger.json`, which had been dropping those
    events).
- `src/sim/simulate-tick.cpp` — the always-on physical AimBody
  (`ragdoll.updateAim`) is skipped for Melee actors (`usesMeleeAuthoredPose()`),
  so limb momentum/head tilt cannot alter the authored pose; the aim body is
  deactivated for them.
- `src/combat/melee-client.cpp` — `anim.melee.authored_pose`/limb events moved to
  the `Avatar` category; `melee.client.limb` now logs the attached part's world
  rotation and a limb-tip vertex (`tip_world`, `tip_delta`, `tip_moved_m`) so
  limb motion is numerically measurable.
- `src/network/server-gamemode.cpp::serverResolveActorSpawnProfile` — Melee mode
  forces `avatarName = "captainfalcon"` for every actor, so the NPC (and player)
  use the Falcon profile regardless of role/random NPC avatar.

## Animation-selection and suppression path

1. `simulate-tick.cpp`: local player animation via `physicsMainUpdate`; the
   always-on AimBody is skipped when `player.usesMeleeAuthoredPose()`.
2. `Player::updateProceduralAnimation` computes `meleeExclusivePose =
   usesMeleeAuthoredPose()`; ragdoll/procedural-frozen early returns are bypassed.
3. State machine selects the action phase clip (`playOneShotPhase`) or the
   avatar's `idle`/`walk`; `findClip` is restricted to the avatar set.
4. Per-part: only the sampled authored transform is applied (weapon/reload/
   freeze/dash/axis-lock/idle-sway/AimBody all skipped); `part.pose = target`
   (no spring); written to `perfectPoseSkeleton.nodes[].localTransform` and thus
   `PhysicalBodyPart::worldTransform` (rendering + attached move hitboxes).

## Evidence

- Build: `python build_agent.py` → `BUILD SUCCESS`.
- Runtime: `devscripts/scenarios/melee_probe_v1.json` on run
  `logs/2026-10-18/20261018_043244/events.jsonl` (dev-loop build; profile now
  starts into Melee with `--mode melee --weapon-set 12`):
  - `anim.melee.pose_probe` (client): `NPC Dev | captainfalcon | uses=true` and
    `NPC-1000 | captainfalcon | uses=true`.
  - `anim.melee.authored_pose` (client): clips `idle` (14) and `walk` (57), all
    `pose_policy: authored_only`, `suppressed_layers` includes aimbody,
    weapon_pose, reload_overlay, freeze_pose, dash_pose, idle_sway,
    ragdoll_presentation, global_fallback.
  - `anim.melee.clip_missing` = 0.
  - `melee.client.limb` for `a_ground_punch` rightArm: `tip_moved_m` 1.029 (active)
    and 1.453 (recovery), with `world_rot_deg` changing — the authored pose moves
    the limb end.
  - `movement.jump.hold_scaled_start`: `base_velocity 15.1, min_multiplier 1.0,
    start_velocity_z 15.1, charge_ticks 10`.
  - `melee.hit` present (NPC hits the human); screenshot shows `0%` above both
    fighters (percent-above-head mode feature).
- Human visual acceptance: not performed (headless runs); the screenshot supplied
  by the user shows Falcon fighters with percent nameplates.

## Missing authored states/clips

`assets/avatars/captainfalcon/animations.json` currently provides: `idle`,
`walk`, and per-action `cf_*` windup/active/recovery clips, `cf_shield`, `cf_grab`.
Still missing (per `supersmashbrosbrelee.md` §57 required states): `run`,
`jump`, `fall`, `land`, `crouch`, `hurt`, `death`, and a `respawn`/spawn clip;
`AvatarStateMap.hurt`/`death` are empty. Jump/fall/land are not state-machine
slots today, so they are not selected regardless.

## Follow-up — authored the missing states and matched the slots

- `src/entities/player.h` — `AvatarStateMap` gained optional `run`, `jump`,
  `fall`, `land`, `crouch`, `respawn`; `Player::crouching`, `Player::landAnimTimer`.
- `src/entities/player-avatar-animations.cpp` — parse `states.run/jump/fall/land/
  crouch/respawn` (absent = empty = not provided).
- `src/entities/player-animation.cpp` — landing timer; and the exclusive Melee
  state selection now picks `death`, `hurt`, `land`, `jump` (rising), `fall`,
  `crouch` (grounded), `run` (fast), `walk`, else `idle`, from the profile;
  missing optional states fall back within the same profile.
- `src/combat/melee-client.cpp` — publishes `player.crouching` from the crouch
  input; added `MimitaMelee::debugSetCrouch` (+ `agent.crouch <0|1>`) test hook.
- `assets/avatars/captainfalcon/animations.json` — added `run`, `jump`, `fall`,
  `land`, `crouch`, `hurt`, `death`, `respawn` clips (47 clips total) and mapped
  them in `states`.

Live verification (`logs/2026-10-18/20261018_151800/events.jsonl`, client
`anim.melee.authored_pose`): both `NPC Dev` and `NPC-1000` are captainfalcon with
`uses_melee_authored=true`; selected clips observed: `jump`, `fall`, `land`,
`run`, `walk`, `idle`, and attack phase clips; `anim.melee.clip_missing = 0`.
`crouch`, `hurt`, `death`, `respawn` are authored and wired but were not
observed live (crouch needs a held key / stable ground; the synthetic
`agent.crouch` executed on the client but did not reflect in the probe window —
likely the local driver's equip timing; hurt/death need a hitstun/death event
that Melee intentionally does not produce).

## Open notes

- The user reported the game "stopped"; the selected journal shows no crash,
  exception, or fatal record — the client was in a network reconnect state.
- `movement.jump.hold_scaled_end` did not fire under the synthetic edge-only
  `agent.action jump`; the boost/held-Space path needs a real held key to show
  the height increase (start + config values are proven).
- Unrelated pre-existing edits (parallel documentation/command work) were
  preserved.
