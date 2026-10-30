// 2026-10-30T01:37:00Z
/* purpose
* record the gamemode resolver facade, per-mode aim/feedback fields, Juggernaut
* NPC combat fixes, and target-relative approach sectors
* separate source/build/runtime evidence from human acceptance
*/

# Task

- Task ID: gamemode-resolver-npc-combat-approach-20261030
- Summary: Add masterlist-backed per-mode aim/body/victim-feedback fields, a
  single `GamemodeRuntimeResolver` client apply/clear owner, restore persistent
  NPC pursuit, give Juggernaut roles authored weapon cycles, make roster
  difficulty data-driven, and add target-relative approach sectors.
- Status: Implemented; build and server runtime evidence collected; human
  visual/gameplay acceptance still open.
- Date, time, timezone: 2026-10-30T01:37:00Z, ISO 8601
- Branch: 20261011zt
- Base commit: 1681b393
- Final commit: not created by this session

# Pre-existing changes

- Exact status output (selected): the working tree already contained modified
  melee/combat, ragdoll, input, main-systems, networking-contact, scenario, and
  config files before this session.
- Files not created or modified by this session: `src/combat/melee-client.*`,
  `src/combat/weapon-json-config.cpp`, `src/combat/weapon-types.h`,
  `src/engine/engine-tick-combat.cpp`, `src/input/*`, `src/main-systems.cpp`,
  `src/ragdoll/*`, `src/sim/simulate-tick.cpp`,
  `src/network/server-physical-contact-actors.cpp`,
  `config/accounts/default.json`, `config/analytics.json`,
  `config/crosshair.json`, `config/weapons/*`, and the unrelated docs/scenario
  edits. They were preserved.

# Requested behavior

- Juggernaut NPCs/Fighters must use their full role weapon set, dash, and keep
  pursuing a target instead of forgetting it.
- `aimbody.json`/`gameplay.json` behavior must be selectable per gamemode
  through the gamemode masterlist, not separate override blocks in those files.
- Hit flinch and victim-screen blood feedback must be per-gamemode.
- NPCs should approach from different sides relative to the target's look and
  reassign on a configured interval.
- Sandbox's raw-JSON configuration source must be documented as an open
  decision.

# Specification alignment

- Current specification paths:
  `docs/architecture/json-configuration/json-configuration.md`,
  `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/gamemodes/juggernaut.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/architecture/player-npc-systems/npc-movement.md`,
  `docs/specs/open-gameplay-issues.md`.
- Exact requirements: these documents were updated first to define the resolver,
  the nullable `aim` fields, `presentation.blood_client_feedback`, persistent
  pursuit, role weapon cycles, data-driven difficulty, and sector approach.
- Why the change follows the specification: the schema is defined in the
  masterlist, values live per mode, one resolver applies them, and the NPC
  systems remain the existing profile/TeamBrain/executor owners.
- Conflicts or decisions: movement precedence and full per-singleton flag
  deletion remain follow-ups; the resolver facade lands first.

# Exact implementation changes

## File: `config/gamemodes/masterlist/gamemode-masterlist.json`

- Added nullable `aim.mode`, `aim.body_mode`, `aim.body_enabled` and
  `presentation.blood_client_feedback` schema entries.
- `gamemode_json_sync.py --write` added the fields as `null` to every mode;
  `--check` passes.

## File: `config/gamemodes/juggernaut.json`

- `aim.mode: physical`, `aim.body_mode: hybrid`, `aim.body_enabled: true`,
  `presentation.hit_flinch: true`, `presentation.blood_client_feedback: true`.

## Files: `src/gamemode/gamemode.h`, `src/gamemode/gamemode.cpp`

- Added `GamemodeAimPolicy` and `GamemodePresentation.bloodClientFeedback`.
- Parsed the nullable `aim` block and `blood_client_feedback`.

## Files: `src/gamemode/gamemode-runtime-resolver.h/.cpp` (new)

- One client apply/clear owner for healthbars, outlines/trails, hit flinch,
  killfeed, ActorPresetWeapons presentation, aim mode, aim-body mode/enabled,
  and victim blood feedback.
- Applies via existing in-memory setters; base JSON files are never rewritten.

## Files: `src/components` equivalents

- `src/config/gameplay-config.h`: in-memory aim-mode override.
- `src/entities/aimbody-config.h`: in-memory mode/enabled overrides; `save()`
  still round-trips the authored values.
- `src/config/impact-decals-config.h/.cpp`: independent client-feedback
  override mirroring the existing blood override.

## File: `src/network/community-match-client.cpp`

- Replaced the scattered mode apply/clear blocks with one
  `GamemodeRuntimeResolver::applyClient` / `clearClient` call.
- Extended the applied fingerprint with mode and effective aim/body/feedback
  values.

## File: `src/game/game-cli.cpp`

- `--gamemode-selftest` now asserts Juggernaut `aim.mode=physical`,
  `aim.body_mode=hybrid`, `aim.body_enabled=true`.

## Files: `config/behavior-profiles.json`, `src/npc/npc-behavior.*`

- `juggernaut_limited`, `nervous`, `nervousbad`, beginner, and idle no longer
  forget targets; they use memory and persistent pursuit.
- `juggernaut_swarm` now cycles shotgun/revolver/rocket_launcher/spyknife.
- `juggernaut_limited` now cycles heavy_shotgun/large_machine_gun/force_punch.
- Added data-driven `difficulty` (Juggernaut Fighter 8, Juggernaut 6) and
  `applySkillTier` defaults were aligned with persistent pursuit.

## Files: `src/npc/team-brain.*`, `src/npc/npc-utility.h`,
`src/npc/npc.cpp`, `src/npc/npc-states.cpp`,
`src/network/server-gamemode.cpp`

- TeamBrain assigns balanced target-relative sectors (front/right/back/left)
  from the focus actor's look, re-drawn every `approach_reassign_ticks` with
  `slot_stickiness`.
- Server publishes sector direction to `UtilityContext`; navigator/state
  movement stage toward the assigned side while aim target stays unchanged.
- Added bounded `npc.dash` (once per life) and `npc.squad-sector-assigned`
  diagnostics.

# Diagnostics

- Owner/category: `NPC_MOVEMENT`, `GENERAL`.
- Input: resolved mode values, behavior profile, target distance, dash result.
- Decision: apply mode policy; assign sectors; choose weapon visit; dash.
- Output: `client.gamemode.presentation.applied`, `npc.weapon-cycle`,
  `npc.dash`, `npc.squad-sector-assigned`, `npc.pursuit-state`.
- Failure or rejection reason: existing `npc.fire-blocked`/config events remain.
- Rate limiting: dash once per life; sector event once per reassignment per
  team; weapon cycle only on switch/visit edge.

# Validation

- Focused skill paths and results: spec-behavior review applied by updating
  docs before code; no separate skill run recorded.
- Tests and exact commands:
  - `python devscripts/gamemode_json_sync.py --check` -> PASS.
  - `.dev/current/mimita.exe --gamemode-config-validate` -> `masterlist=1
    loaded=16 errors=0`, PASS.
  - `.dev/current/mimita.exe --gamemode-selftest` ->
    `juggernaut aim=physical body=hybrid enabled=1`, PASS.
  - `python devscripts/config_selftest.py` -> FAIL, but only the pre-existing
    avatar validation output is present; no gamemode errors. This is not
    attributed to this change.
- Build status: dev-loop builds 3814, 3815, 3816, 3818, 3819, 3821 succeeded;
  final verified build 3821.
- Runtime or hot-reload evidence: dev-loop launch mode 9 (Juggernaut,
  `dust2cyberiav4`), journal
  `logs/2026-10-30/20261030_013558/events.jsonl`:
  - 934 `npc.weapon-cycle` records; Fighters switch shotgun -> revolver ->
    rocket_launcher and Juggernauts switch heavy_shotgun/large_machine_gun.
  - 63 `npc.dash` records with `difficulty=8.0` and
    `profile=juggernaut_swarm`.
  - 41 `npc.squad-sector-assigned` records; example distribution
    `{"0":11,"1":12,"2":13,"3":12}` across four sectors.
  - `remembered:true` pursuit records show targets retained after LOS loss.
  - `client.gamemode.presentation.applied` shows
    `effective_aim_mode=physical`, `effective_aim_body_mode=hybrid`,
    `effective_aim_body_enabled=true`,
    `effective_blood_client_feedback=true`.
- Output files: current run journal above; build artifacts under `.dev/builds`.

# Measured evidence

- Before values: Juggernaut mode had no per-mode aim/body/feedback fields;
  Fighters used revolver-only scored switching and no authored cycle; roster
  difficulty was hardcoded `1.0`.
- After values: authored cycles, `difficulty=8.0`, balanced 4-sector assignment,
  and effective physical/hybrid/client-feedback values in the journal.
- Timestamps: run started `2026-10-30T01:35:58Z`; samples span roughly
  `t=3s` to `t=70s`.
- Tick/frame/network measurements: not measured for this change.

# Regression review

- Regression entry appended: no
- Why this is or is not a confirmed regression: no previously working behavior
  was confirmed broken. The change was requested behavior.
- Related regression paths: none.

# Human acceptance

- Visual review: not performed by this session.
- Gameplay review: not performed; the human must confirm Fighters visibly dash,
  cycle weapons, spread around the player, and that the Juggernaut feels
  physical/hybrid with victim feedback.
- Multiplayer review: not performed beyond the local listen client/server run.
- Still unverified: per-singleton flag deletion, movement precedence
  consolidation, Sandbox preset decision, and the pre-existing avatar
  validation failures.

# Related feature record

- Feature path: none created; related docs listed above.
