# Handoff: body rig done, ADS / fullscreen-6 / damage / bat / NPC / slope remain

Written 2026-11-27 by `opencode-deepseek-v4-2`, the top-level continuation chat of
`docs/handoffs/20261127_jitter-body-ads-damage-handoff.md`. Read `AGENTS.md`,
`docs/ROUTER.md`, then this document, then the original handoff for the jitter
and outline history. The working tree is the artifact; nothing here is committed.

## Task and session identity

- Agent: `opencode-deepseek-v4-2` (top-level OpenCode chat, not a subagent).
- Branch/checkout: `checkpoint/one-checkout-condensation-20261111`, HEAD
  `7176d28a`, root `C:\mimita-v9`.
- Published game DLL: **generation 783**, build attempt
  `game-dll-20261127T065859749721Z-de4e7c`, status `ok`, `compiled: 4`,
  `total_seconds: 37.373`. Run it through the current EXE build
  `2026-11-27T04:28:12.568764Z:84efd6f35ae7e46c5c2e6efdeefc42a7b4958a19db057da1c94f365c24a3e602`.
- Proof record: task `body-rig-sockets`, id `task-proof-3063cf9b1c71`,
  result `proven`, generation 783, run `20261127_070038`.

## Current state

| Item | State | Evidence |
|---|---|---|
| 1. Per-avatar body sockets + 1:1 limbs + rig editor | **DONE and runtime-proven** | gen 783, run `20261127_070038` |
| 2. ADS arm lock, torso/head aim, slower blend | not started | full pose is header-invasive; see plan |
| 3. Fullscreen 6 all-UI stretched | owned by `hud-fs6-crosshair` | do not edit their files |
| 4. Damage mode 1/2 + authoritative networking | not started | large |
| 5. Bat first-person viewmodel | not started | config looks correct; suspect code path |
| 6. NPC ranged shooting | not started | `npc.fire-blocked` diagnostics exist |
| 7. `collision.json` walkableSlopeDot wiring | not started | see plan |

### Item 1 — what was verified and fixed

The gen-782 partial implementation was inspected and verified live, then one real
bug was found and fixed:

- Body hierarchy conversion is correct. `rig.verdict` in run `20261127_070038`
  (gen 783) is `valid: true` for all three body types in one run:
  - boy `abusiveboy` / `mimita-char-no-animations-v4.glb`:
    `attachment_error_max_m` ~5.96e-08, `torso_scale_pct` 100,
    `child_follows_torso_m` ~0.1436.
  - girl `abusivegirl3` / `mimita-girl-v5.glb`: attachment ~2.98e-08,
    follows ~0.1948.
  - big-girl `girllargev3` / `girl-large-mimita-v10.glb`: attachment ~1.33e-07,
    `torso_scale_pct` 100, follows ~0.3160.
  - `rig.hierarchy` shows `root=plrOrigin`, `torso=torso`, `converted=5`,
    `children=[0,1,3,2,4]`.
- Rig editor commands all execute (`terminal.command.completed`):
  `rig_editor [0|1]`, `rig_editor_select`, `rig_editor_move`,
  `rig_editor_rot`, `rig_editor_save`, `rig_editor_reset`, `rig_editor_print`.
  `rig.editor`, `rig.socket.edited` (moved and rotated), and `rig.socket.saved`
  events fire.
- **Bug fixed:** `rig_editor_save` previously wrote edits under whichever avatar
  loaded last (observed: saved the local big-girl edit under NPC `boiledboy`)
  because the editor read the global `gRigAvatarKey`/`gRigModelKey`. Fix:
  `src/entities/player-loader.cpp`
  - `syncRigEditorKeys(player)` derives keys from the edited player
    (`player.avatarName()`, `player.renderModelKey()`), called by every editor
    mutator/print.
  - `applyAvatarBodyRig` now sets `gRigModelKey` from `player.renderModelKey()`
    so save and lookup share one model key; `emitRigLayoutEvents` also records a
    human-readable `model_path`.
  - `Player::loadModel` miss path now calls `setRenderModelKeyFromPath(cacheKey)`
    before `applyAvatarBodyRig` so the key exists on first load.
- Re-verified at gen 783, run `20261127_070038`: after `avatar.load girllargev3`,
  `rig_editor move/rot/save` produced `rig.socket.saved {key: "girllargev3",
  section: "avatars", parts: 5}`, and `config/rig-sockets.json` contained only
  `avatars.girllargev3`. Reloading `girllargev3` applied the saved socket:
  head offset y `1.2606351`, `source: "avatar"` (evidence the per-avatar
  override is read back, not just written). Test config was restored afterward;
  `config/rig-sockets.json` is back to `{"avatars":{}, "models":{}}`.

Evidence paths:
- Client journal: `logs/2026-11-27/20261127_070038/client-01/events.jsonl`.
- Build summary: `.dev/build-timings/game-dll-20261127T065859749721Z-de4e7c.json`.
- Published DLL: `build/game/generation-783/game-g783.dll`.

## Open items, in order

### 2. ADS pose and slower blend (next)

Goal (from the original handoff): both arms locked to the weapon (right hand at
grip, left at foregrip); torso turns toward the aim; head tilts right/down into
the sight; keep bodycam smoothing; slow the ADS blend to a configurable
0.30-0.40 s; emit `weapon.ads.pose` with sight alignment, arm-lock, torso/head
angles, and blend time.

Current code:
- `src/engine/engine-tick-combat.cpp:134` sets `player.weaponAimDownSights`.
- `src/combat/weapon-viewmodel.cpp:206-234` is a **first-person camera-space**
  ADS blend (`ads.positionOffset`, `ads.rotationDegrees`) driven by
  `vmcfg->ads.blendRate` (currently `18.0` in
  `config/weapons/bloodtest_ak47/config.json` and
  `config/weapons/bloodtest_judgement/config.json`). It already emits
  `weapon.ads.state`.
- Third-person arms are posed by `WeaponPoseConfig`
  (`src/entities/player.h:306`) via `weaponPoses` in
  `src/entities/player-animation.cpp:1218-1285, 1350-1359`. There is **no ADS
  body-pose variant**; the same pose is used hip and ADS.
- `PlayerProceduralConfig` already has unused
  `torsoAimYawStrength/torsoAimPitchStrength/armAimYawStrength/armAimPitchStrength`
  (`src/entities/player.h:374-377`, loaded by `player-config.cpp:54-57`) but no
  consumer. Use these rather than adding more fields.

Recommended smallest safe path (avoid `player.h`; it has ~229 dependents):
1. Add a per-player or local ADS pose weight with a duration-based blend in
   `player-animation.cpp` (rate = 3 / duration for ~95% in `duration` seconds;
   duration ~0.33 s). Read `player.weaponAimDownSights`.
2. When active and a weapon pose exists, bend the existing
   `weaponPoseCfg->leftArm/rightArm` toward the weapon grip with a smoothed
   weight, and apply `torsoAimYaw/PitchStrength` to torso and a small head
   tilt/pitch into the sight. Reuse the aim values already used for look-pitch at
   skeleton assignment (search `player-animation.cpp` for where look pitch is
   applied after the pose).
3. Emit `weapon.ads.pose` once per state/steady interval with:
   `weapon`, `ads_requested`, `blend`, `sight_alignment_m`,
   `right_hand_grip_m`, `left_hand_foregrip_m`, `torso_yaw_deg`,
   `torso_pitch_deg`, `head_tilt_deg`, `blend_seconds`, `blend_rate`.
4. Slower blend: prefer computing the viewmodel rate from a new
   `ads.blend_seconds` JSON field with fallback to `1/blend_rate`; if you must
   avoid code, setting `blend_rate` to ~8-9 gives ~0.33-0.42 s to 95%, but that
   is config-only and needs live proof, not an unverified claim.
5. Prove in a real run: toggle ADS while holding `bloodtest_ak47`, confirm
   `weapon.ads.pose` shows arm lock + torso/head angles and that the blend takes
   ~0.3-0.4 s. Keep bodycam smoothing (`src/engine/engine-tick-camera.cpp`).

### 3. Fullscreen 6 all-UI stretched

`hud-fs6-crosshair` owns `src/engine/engine-tick-ui-game-hud.cpp`,
`src/engine/engine-tick.cpp`, and `src/gui/ui-system.cpp`. Coordinate; do not
edit those files while that agent is active. Desired: HUD, console,
notifications, popups all rendered into the stretched scene/target (nothing
crisp). Mode 2 UI then receives PostFX (CA/vignette) unless a separate 1024x768
UI target is added; note the tradeoff explicitly.

### 4. Damage mode 1 (arcade) / mode 2 (real life) + networking

Mode 1 keeps current limb health 250/500/1000, limb break, explode, gore. Mode 2:
projectile vs weapon capsule -> deflect + melee-clash ricochet, no body damage,
event `projectile.weapon_deflect`; face/head hit -> chunk removal by
damage/force, meat/bone exposed, event `face.chunk`; server-authoritative
limb/gore/chunk/deflect, clients present only, event `damage.net.verdict`.
Config `damage_mode: 1|2`, live `damage_mode 1|2`, active under any lens. This is
large; keep server authority and do not duplicate existing damage owners.

### 5. Bat first-person viewmodel

`config/weapons/bloodtest_bat/config.json` already has the same `viewmodel` block
as the mace and `model.path = assets/objects/weapons/mimita-bat-v1.glb`,
`pose_id = "mace"`. So this is almost certainly a code-path bug, not config.
Check in order: `WeaponSystem::update/render` calls the viewmodel for the bat
slot (slot 30); `def->modelPath` / `vmcfg->modelPath` non-empty;
`heldMesh.verts`/`vao`; `player.equippedSlot == def->slot`; model not off-screen
(compare transforms with the mace). Add `weapon.viewmodel.state` with
path/loaded/verts/slot/equipped/visible. The bat and mace configs are byte-for-
byte identical in the viewmodel block, so diff the runtime events, not the JSON.

### 6. NPC ranged shooting

`NpcCombat` and `npc.fire-blocked` already exist. In a target mode, read the
`npc.fire-blocked` reason (loadout, target, LOS, fire interval) and fix the gate.
Add `npc.fire.verdict`. Keep server authority.

### 7. `collision.json` walkableSlopeDot wiring

`config/collision.json` has `walkableSlopeDot: 0.80` (with comments saying it
seems dead). `src/physics/config.h:55` defines `inline float
MAX_WALKABLE_SLOPE_DOT = 0.80f;` — it is **not const**, so it can be assigned at
runtime without a header change. It is read directly by
`physics-collision-core.cpp`, `physics-collision-block.cpp`,
`physics-collision-dispatch.cpp`, `actor-triangle-solver.cpp`, `player-ai.cpp`,
and `npc-navigation.h`. `src/config/collision-config.cpp` already parses
`config/collision.json` and reloads it; add a clamped read of `walkableSlopeDot`
there and assign `MAX_WALKABLE_SLOPE_DOT` (plus the movement-config copy at
`src/config/movement-config.cpp:202`). Alternatively delete the dead key. Add
`collision.slope.config` and `collision.slope.classify` events and prove with a
real ramp traversal.

## File ownership

Changed by this session (built into gen 783; do not revert):
- `src/entities/player-loader.cpp` (rig editor per-player keys, model-key order,
  `model_path` in rig events).

Partial body work owned by the continuation chain (already in tree, now proven):
- `src/entities/player.h`, `src/entities/player-loader.cpp`,
  `src/entities/aim-commands.cpp`, `src/debug/debug-visuals-lines.cpp`,
  `src/debug/debug-visuals-scene.cpp`, `config/rig-sockets.json`.

Owned by other active agents — do not edit:
- `hud-fs6-crosshair`: `src/engine/engine-tick-ui-game-hud.cpp`,
  `src/engine/engine-tick.cpp`, `src/gui/ui-system.cpp`.
- Rocket League agents: `src/vehicles/**`, `src/gamemode/arena-ball.*`,
  `src/network/vehicle-*`, `config/maps/arena*`; recent edits to
  `src/input/host-input-fallback.cpp` and `src/vehicles/vehicle-system.cpp`
  were theirs and shipped in gen 783.

## Build and run instructions

- Root dev-loop only. Never invoke a compiler, `build.py`, a module worker, or a
  second loop.
- Build approval is mandatory and single-flight (user rule 2026-11-27): file
  `agent-coord.py build request --kind integration --paths <paths> --issue <id>`,
  get approval from the build owner/peers, then seal ONCE. Do not run competing
  builds. A pending batch window was being coordinated by `rocket-league-full`.
- `python devscripts/build-note.py --changes ... --why ... --files ...` before
  saving compiler inputs. Read `game-generation.ready.postmortem` after a build.
- Isolated proof run:
  `python devscripts/dev-loop.py --start-test-run --test-profile tdm-validation
  --test-launch-mode 41 --test-game-generation <gen>`, then
  `--run-id <id> --client client-01 --command "..."`, then
  `--stop-test-run <id>`.
- Build self-repair: if a link names an undefined symbol for an existing source
  or `compiled: 0`, follow the AGENTS runbook; quarantine the object cache only
  after two attempts fail on different TUs and record it.
- The environment is contended (a Rocket League run was alive at 06:33-07:00Z).
  Start bounded runs, verify the exact PID/journal, and stop only your own runs.

## Coordination state

- Begin/end via `agent-coord.py begin|end --agent <name>`; `sync`/`board` at
  activation; `edit begin` before shared files.
- Build requests open at handoff: `build-request-9110130ce5a2` (mine,
  `player-loader.cpp`, already built into gen 783) and
  `build-request-...` from `rocket-league-full` for vehicle/input. The 07:00Z
  batch-build published gen 783 successfully.
- Do not edit `player-loader.cpp` without coordinating; this session's change is
  compiled and proven.

## Known limits

- Only item 1 is proven. Items 2-7 are not started; do not claim them.
- The rig editor reset path was command-verified but has no dedicated event; if
  you add `rig.socket.reset`, prove base-restoration explicitly. Limb-vs-limb
  collision off, world/hit collision unchanged — verify if a task depends on it.
- `config/rig-sockets.json` was restored to empty after testing; any real
  per-avatar tuning still needs authoring and its own proof.

## 2026-11-27 late update — agent opencode-deepseek-v4-3

Generation 783 remains the proven item-1 baseline. This session added and
proved the following; the working tree is still the artifact.

| Item | State | Evidence |
|---|---|---|
| 2. ADS body blend/torso/head | **partial, not done** | gen 784 run `20261127_072858`, `weapon.ads.pose` + `weapon.ads.state`; arm lock not measurably working |
| 5. Bat first-person viewmodel | **DONE, runtime-proven** | gen 786 run `20261127_074621`, `weapon.viewmodel.state` visible=true |
| 7. `walkableSlopeDot` config wiring | **partial, config only proven** | gen 786 run `20261127_074621`, `collision.slope.config` changed=true; classify+ramp open |

Builds:
- gen 784 `game-dll-20261127T072316157011Z-06c929` (request
  `build-request-c6fd97046c97`, no-response seal), total 68.6 s.
- gen 786 `game-dll-...` published at 07:45:59Z from request
  `build-request-2792da10f482`; total ~? Check
  `.dev/build-timings/` for the exact summary. It includes other agents'
  saved sources, not only this task's files.
- **Gen 785 is broken for servers** (two startup exits during config load).
  Cause was my `CollisionConfig::load` constructing `MovementJsonConfig`,
  whose default builder constructs `CollisionConfig` (recursive static init).
  Fixed by having `CollisionConfig` only assign `MAX_WALKABLE_SLOPE_DOT`,
  `movement-config` default `walkableSlopeDot=0` (follow global), and
  `movement-step` fall back to `MAX_WALKABLE_SLOPE_DOT`. Do not launch gen 785.

Item 2 details:
- `weapon-config.h/cpp` now parse `ads.blend_seconds`; `weapon-viewmodel.cpp`
  computes `rate = 3 / blend_seconds`; AK-47 and Judgment use 0.33 s.
- `player-animation.cpp` adds a per-Player ADS gesture blend (bounded static
  map), torso yaw/pitch, head tilt, arm aim, and `weapon.ads.pose`.
- Proven in run `20261127_072858` (gen 784): blend reached 0.99985,
  blend_seconds 0.33, blend_rate 9.0909, head_tilt -7.9988, torso_pitch
  -2.3582, bodycam_smoothing true.
- **Residual:** measured `right_hand_grip_m` and `left_hand_foregrip_m` were
  0.46-6.6 m, `sight_alignment_m` 2.4-7.3 m. The two-hand lock is not solved;
  the measurement may also be using stale/incorrect world transforms. Keep
  issue `issue-9c05cbcd5b0d` open.

Item 5 root cause and fix:
- `bloodtest_bat` and `captain_falcon_moveset` both declared slot 30.
  `getDefForSlot(30)` returned `captain_falcon_moveset`, whose viewmodel is
  disabled and model path is empty, so the bat model never loaded.
- Changed `config/weapons/captain_falcon_moveset/config.json` slot to 31
  (config hot-reload; no rebuild needed). `bloodtest_bat` stays slot 30.
- Added `weapon.viewmodel.state` in `weapon-viewmodel.cpp`.
- Proven in run `20261127_074621` (gen 786):
  `weapon.viewmodel.state` weapon=bloodtest_bat, loaded=true, verts=222,
  vao=9, equipped=true, visible=true,
  loaded_path=assets/objects/weapons/mimita-bat-v1.glb.
  Issue `issue-f5430e84b28b` closed with proof task `bat-viewmodel`.
  Human visual acceptance of the bat's look is still separate.

Item 7 details:
- `collision-config.cpp` clamps `walkableSlopeDot` 0..1 and assigns
  `MAX_WALKABLE_SLOPE_DOT`, emitting `collision.slope.config`.
- `movement-config.cpp` default is now `walkableSlopeDot = 0` meaning "follow
  the shared global"; `movement-step.cpp` falls back to
  `MAX_WALKABLE_SLOPE_DOT`.
- Proven hot-reload in run `20261127_074621`: 0.80→0.85 gave changed=true with
  applied 0.8500000238, then 0.85→0.80 gave changed=true with applied 0.8.
- Still open: `collision.slope.classify` event and a real ramp traversal.
  Issue `issue-9c865462d90d` remains open (`needs-work`).

Not started / owned elsewhere:
- Item 3 fullscreen-6 all-UI stretched: `hud-fs6-crosshair` files remain
  off-limits (`engine-tick-ui-game-hud.cpp`, `engine-tick.cpp`,
  `ui-system.cpp`).
- Item 4 damage modes 1/2 and authoritative networking: not started.
- Item 6 NPC ranged shooting: no `npc.fire`/`npc.fire-blocked` events appeared
  in the TDM run; not investigated beyond that.

Temporary test profile `devscripts/dev-profiles/ads-ak47-proof.json` was used
and removed. Test runs `20261127_072858`, `20261127_074621`, `20261127_073832`,
and `20261127_073959` are stopped/failed and preserved under `logs/2026-11-27/`.
