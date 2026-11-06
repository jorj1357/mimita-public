# Grenade tick unification, camera-volume effects, and Fun CS NPC objective rush

Time created: 2026-11-01T22:14:33Z
Time last updated: 2026-11-01T23:11:41Z

Status: IMPLEMENTED; SELFTESTS AND SCENARIOS PASS; HUMAN VISUAL REVIEW PENDING

## Goal

Make grenade timing a single tick-owned contract, finish the requested
smoke/fire/frag/darkbang presentation and damage behavior, and make Fun Counter
Strike NPCs commit to one bomb site, recover the bomb, plant, defend, and let CTs
defuse through the existing TeamBrain/utility/navigation owners.

## Specification and documentation

Updated before code:

- `docs/specs/gamemodes/funcounterstrike.md`
  - Added the simulation time rule: authoritative server/client ticks, JSON
    `*_ticks`, derived seconds for human display.
  - Made per-weapon `grenade_effect` the only grenade timing owner.
  - Added frag one-tick damage and 20 m red radius sphere, fire all-actor damage
    and color/alpha ramp, smoke 600-tick lifetime and NPC LOS blocking, and
    Darkbang 1 m full-effect/6 m visual/30-tick world+HUD overlay/audio duck.
  - Added T chosen-site commitment, bomb recovery, post-plant defense, CT
    perception-gated reports and defuse assignment.
- `docs/specs/npc-behavior/npc-behavior.md`
  - Added objective rush/site commitment and grenade hazard avoidance sections,
    including Fun-CS-only rush and dash-when-available.
- `docs/specs/weapons/weapons.md`
  - Replaced the grenade schema with tick fields, visual fields, and the single
    fuse owner rule.
- `docs/specs/effects/effects.md`
  - Added the camera-inside volume overlay primitive and radius indicators.
- `docs/specs/gui/guiv2.md`
  - Added Darkbang HUD darkening, audio duck, and two-decimal timer rules.
- `docs/specs/debug-logging/debug-logging.md`
  - Added grenade/hazard/NPC proof event contract.
- `docs/specs/performance/performance.md`
  - Added the shared hazard snapshot and no-hot-loop-allocation budget.

Regression record:

- `docs/regressions/gameplay/grenade-timing-split-REG-2026-11-01.md`
  - Recorded the split fuse/lifetime ownership as a regression and attempted
    fix with runtime proof.

TODO notes found (not rewritten):

- `docs/specs/gamemodes/counterstrike.md:2247` smoke timer TODO. Suggested
  replacement: authoritative `duration_ticks`; display `remaining_ticks / 60`
  to two decimals; client countdown is presentation-only.
- `docs/specs/gamemodes/counterstrike.md:2422` one navigation owner TODO.
  Suggested replacement: `NpcNavigator` remains the only route owner; rush mode
  supplies commitment and uses shared movement.
- `docs/specs/gamemodes/counterstrike.md:2542` aggregated logging TODO.
  Suggested replacement: define the 60-tick NPC movement summary schema.
- `docs/specs/gamemodes/counterstrike.md:1383` bomb pickup GUI TODO. Suggested
  replacement: cross-reference the new NPC recovery/plant flow and world prompt.
- `docs/specs/gamemodes/counterstrike.md:1431` bomb timer presentation decision.
  Suggested replacement: keep bomb HUD low-information; grenade timers use
  two-decimal world text.

## Code and configuration

### Single grenade timing owner

- `src/combat/weapon-types.h`
  - `WeaponGrenadeEffect` now stores `durationTicks`/`fuseTicks` and visual
    fields; seconds are derived methods.
  - Added `weaponGrenadeTimingConfigured()` and
    `weaponGrenadeFuseSeconds()` as the single derived-fuse boundary.
- `src/combat/grenade-registry.h/.cpp`
  - Registry carries ticks/visual fields and logs `grenade.config.resolved`.
- `src/combat/weapon-json-config.cpp`
  - Parses `duration_ticks`/`fuse_ticks`; legacy seconds convert at load.
- `src/combat/area-effect.h/.cpp`
  - Aging/lifetime use integer ticks; added `Danger` hazard kind and
    `areaEffectSegmentBlocked()` for smoke LOS.
- `src/network/server-gamemode.cpp`
  - Area-effect spawn/tick use ticks; added bounded `grenade.area.tick`.
- `src/network/server-projectiles.cpp`
  - Grenade fuse uses `weaponGrenadeFuseSeconds`; frag spawns a short
    `Danger` hazard and logs `grenade.frag.blast`.
- `src/network/server-npcs.cpp`
  - NPC grenades now use the same `fuse_ticks` instead of `projectileLifetime`.
- `src/network/multiplayer-projectiles.cpp`
  - Client prediction fuse uses the same boundary; client volumes age in ticks
    and display two-decimal seconds.
- `src/combat/weapon-grenade-launcher.cpp`
  - Local persistent grenades use the same fuse boundary.
- Config: `config/weapons/{frag,smoke,fire,darkbang}/config.json`
  - `duration_ticks`/`fuse_ticks`; `custom_params.fuseTime` removed.
  - Larger projectile radius 0.6 and stronger bounce
    (`restitution 0.82`, `friction 0.12`, 30 bounces).
  - Frag visual radius 20 m, 30 ticks, `0.5 -> 0.0`.
  - Darkbang visual radius 6 m, 30 ticks, `world_hud`, full effect within 1 m.
  - Fire `damages_enemies_only: false`.

### Presentation

- `src/debug/debug-visuals-render.cpp/.h`
  - Added the shared camera-inside volume overlay accumulator.
- `src/effects/effect-part.h/.cpp`
  - Added `useEndAlpha` and camera-overlay submission for translucent spheres.
- `src/effects/effect-part-render.cpp`
  - Honors `useEndAlpha`; submits camera overlays for radius spheres.
- `src/engine/engine-tick-render.cpp`
  - Resets overlays each frame, draws the world overlay after effects, and uses
    the shared fullscreen overlay helper for spawn flash/darkbang.
- `src/engine/engine-tick-ui.cpp`
  - Draws the HUD-layer overlay after the HUD for `world_hud` volumes.
- `src/engine/engine-tick-state.cpp`
  - Drives global audio duck from Darkbang intensity.
- `src/audio/audio.h/.cpp`
  - Added global duck applied to all normal sound playback; Darkbang apply sound
    temporarily bypasses it.
- `src/network/multiplayer-projectiles.cpp`
  - Smoke/fire volumes submit camera overlays and render two-decimal timers.
  - Frag/darkbang radius spheres spawn through `EffectPart`.

### NPC team and objective behavior

- `src/npc/team-brain.h/.cpp`
  - Added round-scoped chosen T site (`chooseAttackSite`) with deterministic
    seed + round selection and `team.site-choice` logging.
  - T supporters now travel to the chosen site, not the carrier body.
  - Dropped bomb sends only the designated recoverer to the bomb.
  - CTs get no round-site knowledge; `objectiveTargetPosition` uses
    perception-gated team reports, then nearest site.
  - Added `setAssignment`; server assigns one stable CT `Defuse`.
- `src/network/server-gamemode.cpp`
  - Chooses the T round site once.
  - Reports only perceptible enemies (`npc.team-report`), not every actor.
  - Sets `bombDropped`, recoverer, defuser, planted defense context, and planar
    `atObjective`.
  - Publishes one shared hazard snapshot to `NpcSystem`.
  - Removed the redundant `atObjective` plant/defuse gate that could differ from
    the site floor.
  - Stabilized recoverer/defuser selection and stopped per-tick assignment spam.
- `src/npc/npc-utility.h/.cpp`
  - Added team/planted/recovery/defuse/hazard context.
  - T pre-plant suppresses `HuntArea`; T post-plant may chase reports only inside
    the bomb defend radius; CT defuse/retake outranks hunting.
  - Fire/darkbang/frag hazards raise `Survive`.
- `src/npc/npc.cpp`
  - `Survive` flees a hazard position.
  - Smoke blocks NPC LOS through the shared segment query.
  - Fun CS rush dashes whenever the shared ability is available and logs a
    bounded `npc.rush` sample.
- `src/npc/npc-behavior.h/.cpp` and `config/behavior-profiles.json`
  - Added `objective_rush` and the Fun-CS `fun_cs_rush` profile.
- `config/roles.json`
  - Fun CS roles use `fun_cs_rush`; Counter Strike typical keeps `rage2`.

### Scenarios

- `devscripts/scenarios/grenade_tick_contract_v1.json`
  - 13 assertions proving resolved fuse/duration/visual ticks and area-effect
    spawn durations.
- `devscripts/scenarios/fun_cs_npc_objective_v1.json`
  - 5 assertions proving site choice, rush, NPC plant, CT defuser assignment,
    and no unresolved objective.

## Validation evidence

### Build

- Canonical dev-loop build `192` succeeded.
- Executable: `C:\mimita-v9\.dev\builds\0192\mimita.exe`.
- `--versioninfo`:
  `EVENTS_JSONL_PATH=logs/11-01-2026/20261101_180935/events.jsonl`.

### Build-compatibility fixes for pre-existing uncommitted work

The working tree already contained an unfinished `PlayerAiController` camera
signature change and an expanded `AttackRequestPacket`. To make the canonical
build possible without reverting that unrelated work:

- `src/input/input-poll.h/.cpp`: `buildInputFrame` now takes `Camera&` because
  the existing player-AI frame builder mutates the camera yaw/pitch.
- `src/network/packets.h`: the `AttackRequestPacket` static_assert cap is now
  384 bytes to match the existing 8-target claimed-hit payload.

These are compile/build compatibility changes only; they do not add gameplay
behavior.

### Selftests

- `--grenade-selftest`: PASS (4 definitions).
- `--area-effect-selftest`: PASS (`fire_cadence`, `expiry`, `smoke_segment`).
- `--team-brain-selftest`: PASS.
- `--objective-selftest`: PASS.
- `--gamemode-config-validate`: PASS (`masterlist=1 loaded=19 errors=0`).
- `python devscripts/weapon_json_sync.py --check`: `definitions=34 errors=0`.
- `python devscripts/gamemode_json_sync.py --check`: PASS.
- `python devscripts/config_selftest.py`: still FAIL from the pre-existing
  avatar-schema findings, not from these changes.
- `python devscripts/quality/lint.py --gate`: report-only,
  `report_failures=5`, all in pre-existing damage-authority/contact-ownership/
  dead-code/duplicate/coverage categories.

### Runtime scenarios

- `grenade_tick_contract_v1`: PASS, 13 assertions.
  Journal `C:\mimita-v9\logs\2026-11-01\20261101_230947\events.jsonl`.
- `fun_cs_npc_objective_v1`: PASS, 5 assertions.
  Journal `C:\mimita-v9\logs\2026-11-01\20261101_230439\events.jsonl`.

### Live journal evidence

From `20261101_230439` (Fun Counter Strike, `dust2cyberiav4`):

- 6 `team.site-choice` records; the T round site was stable per round.
- 951 bounded `npc.rush` records with objective positions and
  `dash_available:true`.
- 3 `objective.planted` records at site A.
- 4 stable `npc.defuse-assigned` records.
- 13 `grenade.smoke.block` records proving smoke LOS blocking.
- 66 `grenade.frag.blast` records with radius 20 m and `damage_ticks:1`.
- 66 `grenade.frag.blast.visual` records (radius 20, alpha 0.5 -> 0.0, 30 ticks).
- 34 `grenade.darkbang.visual` records (radius 6, `world_hud`, 30 ticks).
- 821 `grenade.area.tick` records with `remaining_ticks` and derived seconds.
- 572 `grenade.effect.damage` records.
- 104 `npc.hazard-avoid` records, including live frag projectiles as
  `hazard_kind:4`, `radius_m:20`.
- Fire effect 16 owner 100018 also damaged actor 100018, proving thrower damage.

From `20261101_215432` (same mode/map):

- `objective.plant-start` by NPC 100011 at site A, then `objective.planted`.
- `npc.defuse-assigned` and `objective.defused` by the CT team.

From `20261101_214537`:

- `objective.dropped` then `npc.bomb-recovery`, and an `objective.pickup` by a
  Terrorist NPC after it reached the dropped bomb.
- The later recoverer approach fix removes the 2.77 m stop that could leave a
  recoverer outside the 1.5 m pickup radius.

## Human review still required

- Confirm smoke opacity, fade band, and the two-decimal live timer visually.
- Confirm fire orange-to-dark-red ramp and readable sustained damage.
- Confirm the 20 m frag radius sphere and one-tick damage feel.
- Confirm Darkbang world/HUD darkening, 30-tick radius sphere, and audio duck.
- Confirm NPC rush feels fast and direct without visible oscillation.
- Confirm T bomb recovery -> chosen site -> plant and CT defuse behavior in a
  clean long match.
- Confirm multiplayer presentation and non-host authority.

## Pre-existing edits preserved

The working tree already contained unrelated modified files before this session,
including `config/accounts/default.json`, `config/analytics.json`,
`config/audio/music-settings.json`, `config/movement/movement-source.json`, and
the `devscripts/blender-ai` submodule state. They were not reverted or claimed as
this session's work.
