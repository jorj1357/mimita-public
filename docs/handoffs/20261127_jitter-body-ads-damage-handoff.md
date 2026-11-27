# Handoff: jitter, body sockets, ADS, damage modes, viewmodels

Written 2026-11-27 by `opencode-deepseek-v4-1`. The working tree is the
artifact; nothing here is committed. Read `AGENTS.md`, then this document, then
the routed specs for each item.

## Current state

| Item | State | Evidence |
|---|---|---|
| Bounded jitter probe | implemented | `src/debug/jitter-probe.*`; `jitter_probe <seconds>`; run `20261127_054945`, gen 777, ten `jitter.window` + `jitter.verdict` |
| Live animation alpha | already present; not the jitter cause | `engine-tick-replay.cpp` sets `setAnimationRenderAlpha(simClock.alpha())` |
| Fullscreen-6 crosshair centre | implemented by this agent, then superseded by active `hud-fs6-crosshair` edits | `engine-tick-ui-game-hud.cpp` is **owned by `hud-fs6-crosshair` right now** |
| Outline/body pose split | fixed | shared `playerRenderPartTransform`; A/B `player_combined_pose 0/1`; proof run `20261127_011757` |
| Lens mode 5 / ACES | done earlier | `docs/regressions/presentation/...` |
| Animation mode 2 tether | done earlier | `anim_tether` events |
| No-limit lens config | phase 1 done | `config-limits-retirement.md` |
| Body socket hierarchy + rig editor | **source implemented, not runtime-proven** | gen 782; `config/rig-sockets.json`; files below |

> **Partial body work already in the tree.** A short-lived subagent
> implemented the socket hierarchy and rig editor before it was stopped:
> `src/entities/player.h`, `src/entities/player-loader.cpp`,
> `src/entities/aim-commands.cpp`, `src/debug/debug-visuals-lines.cpp`,
> `src/debug/debug-visuals-scene.cpp`, and new `config/rig-sockets.json`.
> Generation 782 compiled successfully and a build note is queued. No test run
> or runtime proof was started. The continuation owner must inspect this diff,
> verify the hierarchy/editor live, fix issues, and either keep or remove it
> based on evidence. Do not blindly revert it.
>
> Commands added: `rig_editor [0|1]`, `rig_editor_select <part>`,
> `rig_editor_move <dx> <dy> <dz>`, `rig_editor_rot <rx> <ry> <rz>`,
> `rig_editor_save`, `rig_editor_reset`, `rig_editor_print`.

Latest relevant generations: **776** (jitter probe), **777** (RL change on top),
**778** (crosshair owner change on top). Use `dev-loop.py --status` and the
latest published generation before pinning a test run.

## Jitter work

The probe samples:

- `interp` — remote interpolation alpha delta, final render delta, hard snaps,
  underruns, holds, buffer depth.
- `actor` — per-part rendered second difference and raw-vs-interpolated lag.
- `camera` — camera position and Euler second difference.
- `hud` — crosshair centre error when a forced centre is active.

Run:

```
jitter_probe 10
```

Evidence from run `20261127_054945` (mode 61, fullscreen 6):
`actor.accel_max_m=0.00248`, `actor.raw_lag_max_m=0.00061`, `camera=0.0`,
`interp=0` (no remote actors), `hud=0` (see ownership note below).

Next jitter step:

1. Run a **team mode with remote actors** (mode 31 TDM or 45 Fun CS) and
   `jitter_probe 10`; fill `interp` and moving `actor` samples.
2. Run a moving/ADS camera case; fill `camera`.
3. Read the `jitter.verdict` categories and fix the largest:
   - `interp` → `multiplayer-interpolation.cpp` filters, snap/hold/underrun.
   - `actor` → `renderPartTransform` / combined path / capture tick.
   - `camera` → `engine-tick-camera.cpp` bodycam spring/ADS.
   - `hud` → projection and the crosshair owner.
4. Keep server authority; do not rewrite network paths blindly.

**HUD sample blocker:** `hud-fs6-crosshair` edited
`engine-tick-ui-game-hud.cpp` concurrently. Their new crosshair code calls
`JitterProbe::sampleHud(..., false)`, so HUD samples are zero. A coordination
question was sent. Do not overwrite their file; pass the real forced-centre
value when they finish.

## Queued work, in order

### 1. Per-avatar body sockets and 1:1 limbs

Current truth:

- The default player GLB is a flat 7-node rig: `plrOrigin` + `torso`, `head`,
  `leftArm`, `rightArm`, `leftLeg`, `rightLeg`, all direct children of
  `plrOrigin`.
- `config/bodyparts.json` repeats root-relative offsets for the default body and
  can override every avatar.
- Limbs are one rigid mesh each; no elbows/knees.
- `renderPartTransform` interpolates previous/current; `drawCombinedBodyParts`
  now uses the shared helper.

Target:

- Torso is the parent of head/arms/legs; socket offsets are torso-relative.
- Per-avatar auto-fit from each avatar GLB; global `bodyparts.json` only as a
  fallback. The girl/big-girl GLBs have different node translations.
- Rotation-only limbs; the animation-mode 2 tether clamps any translation.
- Limb-vs-limb collision off; world/hit collision stays.
- Rig editor: point = socket, line = parent→child, face/plane = orientation;
  drag to move, live preview, save per avatar. Reuse the map-editor handle
  style if practical.

Implementation hint: after the resolved `restLocalTransforms`/`nodes`, reparent
the five part nodes to the torso:
`restLocal[child] = inverse(restLocal[torso]) * restLocal[child]`, set both
`player.nodes[child].parent` and `perfectPoseSkeleton.nodes[child].parent`, and
update parent children lists. Preserve the rest world positions. Add a config
or live A/B toggle while proving.

Events: `rig.socket`, `rig.hierarchy`, `rig.verdict` (attachment error, torso
scale, child-follows-torso). Prove boy, girl, and big-girl bodies.

### 2. ADS pose and slower blend

- Both arms locked to the weapon; right hand at the grip, left at the foregrip.
- Torso turns toward the aim; head tilts right/down into the sight.
- Keep bodycam smoothing.
- Slow the ADS blend: current `blend_rate: 18.0`; use a configurable duration
  around 0.30–0.40 s.
- Event: `weapon.ads.pose` with sight alignment, arm lock, torso/head angles,
  blend time.

### 3. Fullscreen 6: all UI stretched

- Current mode 2 draws HUD after PostFX at 1920x1080 (crisp).
- Owner `hud-fs6-crosshair` is already implementing mode-6 HUD work. Coordinate
  before touching `engine-tick-ui-game-hud.cpp` or `src/gui/ui-system.cpp`.
- Desired: all UI (HUD, console, notifications, popups) into the stretched
  scene/target; nothing crisp. Forced in-scene is accepted; note that mode 2 UI
  then receives PostFX (CA/vignette) unless a separate 1024x768 UI target is
  added.

### 4. Damage mode 1 / 2 and networking

Naming: **damage modes**, independent of lens modes.

- Mode 1 arcade: keep limb health 250/500/1000, limb break, explode, current
  gore.
- Mode 2 real life:
  - projectile vs weapon capsule → deflect, melee-clash ricochet sound, no body
    damage, `projectile.weapon_deflect`;
  - face/head hit → chunk removal based on damage/force, meat/bone exposed,
    `face.chunk`;
  - server-authoritative limb health/gore/chunk/deflect; clients present only;
    `damage.net.verdict`.
- Config `damage_mode: 1|2`, live `damage_mode 1|2`; visible under any lens.

### 5. Bat viewmodel

The bat config already has the same `viewmodel` block as the mace and the model
`assets/objects/weapons/mimita-bat-v1.glb`. Use the normal `WeaponViewModel`
path; do not add a special system. Check in order:

1. `WeaponSystem::update/render` calls the viewmodel for the bat slot.
2. `def->modelPath` / `vmcfg->modelPath` is non-empty.
3. `heldMesh.verts`, `vao`, and `player.equippedSlot == def->slot`.
4. The model is not off-screen; compare transforms with the mace.

Add `weapon.viewmodel.state` with path/loaded/verts/slot/equipped/visible.

### 6. NPC shooting

`NpcCombat` and `npc.fire-blocked` already exist. Read the blocker reason in the
target mode (loadout, target, LOS, fire interval) and fix the gate. Add
`npc.fire.verdict`.

### 7. Slope dot

`config/collision.json` has `walkableSlopeDot`, but `movement-config.cpp` and
`physics-collision-core.cpp` use `MAX_WALKABLE_SLOPE_DOT`. Wire the JSON value
in or delete the dead key. Add `collision.slope.config` and
`collision.slope.classify`.

## Coordination and build rules

- Use the root dev-loop only. No compiler, build script, or second loop.
- Active owners to avoid:
  - `hud-fs6-crosshair` — `src/engine/engine-tick-ui-game-hud.cpp`,
    `src/engine/engine-tick.cpp`, `src/gui/ui-system.cpp`.
  - Rocket League agents — `src/vehicles/**`, `src/gamemode/arena-ball.*`,
    `src/network/vehicle-*`, `config/maps/arena*`.
- Run `agent-coord.py sync` at activation, `begin` before edits, `edit begin`
  for shared files, `activity set` at phase boundaries, `end` when done.
- Build self-repair: if a link names an undefined symbol for an existing source
  or `compiled: 0`, follow the AGENTS.md runbook. The previous cache fault was
  fixed by content edits in `player-ai.cpp` and
  `client-collision-world-view.cpp`; a `.bak` object cache quarantine may be
  needed if two attempts fail on different TUs.
- Isolated test runs: the environment is contended. Use short command
  sequences immediately after `--start-test-run`; the dev-loop control path can
  report `client-01 is not alive` while the journal is still updating. Verify
  the client PID/journal before assuming failure. The user has authorized
  running as many MiMITA EXEs as needed; preserve other agents' runs.

## File map

- Jitter: `src/debug/jitter-probe.*`, `src/debug/debug-diag-commands.cpp`,
  `src/network/multiplayer-interpolation.cpp`,
  `src/engine/engine-tick-camera.cpp`, `src/render/render-player.cpp`.
- Body/pose: `src/entities/player-loader.cpp`, `player-animation.cpp`,
  `player-render.cpp`, `player-render-mesh.cpp`, `player.h`,
  `config/bodyparts.json`, `src/engine/engine-tick-ui-game-hud.cpp` (owned by
  the crosshair agent).
- ADS/viewmodels: `src/combat/weapon-viewmodel.cpp`,
  `src/combat/weapon-system.cpp`, `config/weapons/bloodtest_*`.
- Damage: `src/npc`/`src/combat`/`src/network` owners, `config/weapons`,
  `config/gamemodes/bloodtest.json`.
- Slope: `config/collision.json`, `src/config/movement-config.cpp`,
  `src/physics/movement/physics-collision-core.cpp`.

## Evidence to preserve

- Jitter: run `20261127_054945`, gen 777, `jitter.window`/`jitter.verdict`.
- Outline: run `20261127_011757`, gen 748, A/B mismatch 7.92/5.69 m → 0.0.
- Crosshair: run `20261127_011550`, gen 748, fullscreen 6 modes 2/5 centered.
- Lens/CA: `docs/regressions/presentation/...`.
- Changelog: `docs/changelog/2026-11-26/20261126-bloodtest-presentation-pass2.md`.

## 2026-11-27 late update: build approval, postmortem, continuation chat

### Build approval is now mandatory

AGENTS.md now requires one shared build at a time with an approved request:

1. Queue the note with `build-note.py --changes ... --why ... --expected-seconds ...`.
2. `agent-coord.py build request --kind integration --paths <changed> --issue <id>`.
3. Build owner approves with `agent-coord.py build approve <request>`. If no
   owner is set, two active non-requesters approve. A requester cannot
   self-approve unless it is the build owner.
4. Only `approved` requests build:
   `dev-loop.py --request agent-build --build-request <request>`.
5. If no answer within 5 minutes, post one final notice; after 10 minutes of
   silence with no active build/halt/freeze, self-approve and record the
   timeout.
6. After every build, read `game-generation.ready.postmortem`, append the
   measured cause to the issue, and act on the largest cause before the next
   build.

### Build why / postmortem

- `build-note.py` now accepts `--why`, `--header-fanout`,
  `--expected-seconds`, `--risk`, and `--proof`. `--why` is required by policy;
  missing why is flagged in the postmortem.
- The dev-loop now adds a `postmortem` block to the build summary and to
  `dev-loop.game-generation.ready`:
  - why, agent, attempt/generation, changed paths/headers, header fanout;
  - total/worker/compile/link/publish/object-scan seconds;
  - max/mean per-TU queue wait;
  - compiled/skipped/cache invalidations;
  - top 5 slowest TUs;
  - mean/peak CPU and minimum free memory;
  - ordered suggestions and metrics to watch.
- Spec: `docs/specs/dev-loop/build-why-and-postmortem.md`.
- Example from gen 782: 603s total, 588.7s compile, 568s max queue wait, CPU
  99.8%, minimum free memory 49 MB, `player.h` changed without a fanout record.
  That is the resource-pressure/header-fanout case the next agent must act on.

### Process autonomy

Agents may start as many checkout-owned MiMITA EXEs and isolated test runs as
needed, through the root dev-loop, recording run IDs and cleaning up only their
own processes. Issue: `issue-44bdee5f2121`.

### Continuation

The handoff was started as a **new top-level OpenCode chat**, not a subagent:
`wake-9ad211184399`, agent `opencode-deepseek-v4-2`, child PID 27560. It was
given this document and the autonomy/build-approval update. A temporary
subagent was stopped before it could prove anything; its partial body-socket
work remains in the tree at gen 782 and must be verified first.
