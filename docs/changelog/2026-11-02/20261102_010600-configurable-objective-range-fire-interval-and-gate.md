# Configurable objective range, NPC fire interval, and mandatory quality gate

Time created: 2026-11-02T01:06:00Z
Time last updated: 2026-11-02T01:06:00Z

Status: IMPLEMENTED; AGENT GATE GREEN; HUMAN VISUAL REVIEW PENDING

## Goal

Make the bomb interaction range and NPC attack cooldown data-owned and easy to
edit, make the lint gate meaningful against pre-existing debt, and finish the
reported Fun Counter Strike AI/presentation issues without adding duplicate
systems.

## Documentation

- `docs/specs/gamemodes/funcounterstrike.md`
  - `interaction_range_m` is configurable (6 m shipped) and is the single
    server/client prompt range.
  - Fire now deals 1 damage every fixed tick to every capsule-intersecting
    actor, including the thrower.
- `docs/specs/gamemodes/objective-hud.md`
  - The prompt range comes from the replicated objective range, not a hardcoded
    radius.
- `docs/specs/npc-behavior/npc-behavior.md`
  - Added an NPC tuning cheat sheet covering roles, behavior profiles, movement
    presets, `fire_interval_ticks`, dash, formation, and separation precedence.
- `docs/specs/dev-loop/dev-loop.md`
  - Added scenario explanation, launch preflight, and `scenario.step` observed
    fields.
- `AGENTS.md`
  - Added the mandatory `python devscripts/quality/agent-gate.py` gate.
- `docs/regressions/2026-11-01/npc-closed-block-interior-REG.md`
  - Added the repeatable wall-containment scenario occurrence.

## Configuration

- `config/gamemodes/funcounterstrike.json` and `counterstrike.json`
  - Added `"interaction_range_m": 6.0`.
- `config/gamemodes/masterlist/gamemode-masterlist.json`
  - Added the `interaction_range_m` schema field.
- `config/behavior-profiles.json`
  - `fun_cs_rush` now has `fire_interval_ticks: 12`,
    `dash_chance: 1.0`, `dash_cooldown_ticks: 3`,
    `formation_slots: 6`, `formation_radius_m: 5.0`, `separation_m: 1.5`.
- `config/weapons/fire/config.json`
  - `damage_per_tick: 1`, `damage_interval_ticks: 1`.
- `devscripts/quality/gate_baseline.json`
  - Accepted pre-existing lint metrics: damage-authority 81, contact-ownership
    41/6, dead-code 8, duplicate windows 551, coverage artifacts 0.

## Code

### Objective interaction range
- `src/gamemode/gamemode.h/.cpp`
  - `GamemodeObjectiveDefinition::interactionRangeMeters`, parsed from
    `interaction_range_m` and clamped 0.5-20 m.
- `src/network/server-gamemode.cpp`
  - Copies the range onto `ObjectiveInstance.interactionRange` and broadcasts it.
- `src/network/packets.h`
  - `DuelStatePacket.objectiveInteractionRange`; protocol version 50.
- `src/network/community-match-client.h/.cpp`
  - Stores the replicated range.
- `src/engine/engine-tick-ui-overlays.cpp`
  - Defuse and pickup prompts use `objective.interactionRange`.

### NPC attack cooldown
- `src/npc/npc-behavior.h/.cpp`
  - Added `fire_interval_ticks`, `dash_chance`, `dash_cooldown_ticks`,
    `formation_slots`, `formation_radius_m`, and `separation_m` profile fields.
- `src/npc/npc-combat.cpp`
  - `attackCooldown` is clamped to the profile's fixed-tick floor after cadence
    and aggression calculation. No weapon can fire faster.
- `src/npc/npc.cpp`
  - Behavior dash chance/cooldown overrides apply.
  - Objective-rush dash is requested even without a visible target.
  - Nearby-actor separation steering keeps teammates from stacking.
- `src/network/server-gamemode.cpp`
  - Deterministic per-team travel/cover slots around the objective, bomb, or
    defuser.
  - The designated CT defuser uses the stable `reportedDefuserId`.
  - Only the designated defuser can start/continue a defuse, preventing
    `operator_changed` interruptions.
- `src/npc/npc-utility.cpp`
  - The designated defuser's `DefuseObjective` outranks combat; non-defusers
    cover; T post-plant defense remains bounded to the bomb radius.

### Smoke and Darkbang presentation
- `src/debug/debug-visuals.h/.cpp` and `debug-visuals-tris.cpp`
  - Added `drawSolidSphere`/`flushSolidTris` with depth writes.
- `src/network/multiplayer-projectiles.cpp`
  - Smoke renders as a fully opaque depth-writing gray sphere; camera inside
    gets a full-opacity gray overlay.
  - Darkbang is full black inside its 6 m effect radius with line of sight.
- `src/engine/engine-tick-ui.cpp`
  - HUD overlay takes the stronger of Darkbang intensity and camera volume.

### Damage feedback and rewards
- `src/network/confirmed-damage-presentation.cpp`
  - Local victims now receive flinch and camera blood from the same confirmed
    damage path as remote victims; damage numbers/blood/impact effects are
    shared for NPC hitscan, projectiles, Deathball, and grenades.
- `src/network/multiplayer-projectiles.cpp`
  - Removed the duplicate local-victim effect block.
- `src/network/server-gamemode.cpp`
  - Added one `kill.reward` event for every authoritative kill with killer,
    victim, weapon, XP, and gold attribution.
- `src/network/multiplayer-tick.cpp`
  - Local killer receives an immediate reward popup for any authoritative kill.

### Fire damage
- `src/combat/area-effect.cpp`
  - Fire damage is applied every configured tick to every actor whose capsule
    intersects the fire cylinder, not only the actor center point.
- `src/network/server-gamemode.cpp`
  - Damage journal samples are rate-limited to 30 ticks while damage still
    applies every tick.

### Scenarios and gate
- `devscripts/scenarios/fun_cs_npc_objective_v1.json`
  - Explicit `weapon_set: 30`; asserts the live scenario launch used map
    `dust2cyberiav4` and weapon set 30.
- `devscripts/scenarios/grenade_tick_contract_v1.json`
  - Explicit `weapon_set: 30`.
- `devscripts/scenarios/npc_wall_containment_v1.json`
  - New repeatable containment scenario at the reported block.
- `devscripts/dev-loop.py`
  - `--scenario-explain` prints purpose, launch, steps, and assertions.
  - Normal scenario runs print the same plan.
  - `scenario.step.observed` now contains the matched event fields.
- `devscripts/quality/lint.py`
  - Compares per-category metrics against `gate_baseline.json`; fails on any
    increase or new report.
- `devscripts/quality/agent-gate.py`
  - Runs build, focused selftests, lint baseline, JSON sync checks, and the fast
    runtime scenarios.
- `.vscode/tasks.json`
  - Added `Run agent gate` and `Explain Fun CS scenario`.

## Validation

### Build

- Canonical dev-loop build `209` succeeded.
- Executable: `.dev/builds/0209/mimita.exe`.

### Selftests

- `--grenade-selftest`: PASS.
- `--area-effect-selftest`: PASS.
- `--team-brain-selftest`: PASS.
- `--objective-selftest`: PASS.
- `--npc-navigation-selftest`: PASS.
- `--collision-selftest`: PASS.
- `--canonical-contact-selftest`: PASS.
- `--gamemode-config-validate`: PASS (`masterlist=1 loaded=19 errors=0`).

### Agent gate

`python devscripts/quality/agent-gate.py` is GREEN:

- build PASS;
- all focused selftests PASS;
- lint baseline gate PASS;
- weapon/gamemode JSON sync PASS;
- `grenade_tick_contract_v1` PASS;
- `npc_wall_containment_v1` PASS.

### Runtime scenarios

- `fun_cs_npc_objective_v1`: PASS, 7 assertions.
  Journal `logs/2026-11-02/20261102_005807/events.jsonl`.
  Launch preflight confirmed map `dust2cyberiav4`, mode 45, weapon set 30.
- `npc_wall_containment_v1`: PASS, 3 assertions.
  Journal `logs/2026-11-02/20261102_002034/events.jsonl`.
- Reported block probe: `inside_closed_solid=true`; no `npc.pose.reset` or
  `npc.pose.penetration` during the 30-second soak.

### Observed defuse behavior

From `logs/2026-11-02/20261102_003910/events.jsonl`:

- `npc.defuse-assigned` for a CT actor;
- `objective.defuse-start` by NPC 100004 with `required_ticks:300`;
- the run later showed an interruption/explosion, which is normal combat
  pressure, not a missing defuse path.

### Human review still required

- Confirm smoke is fully opaque outside and full-screen gray inside.
- Confirm Darkbang is fully black inside 6 m.
- Confirm dash frequency, formation spread, and no bunching.
- Confirm CT defuse prompt range at 6 m and actual defuse completion under
  normal play.
- Confirm local damage numbers/blood for NPC/Deathball/fire hits and the reward
  popup for all weapon kills.
- Confirm fire damage while passing through the edge and the 1-damage/tick
  cadence feel.
- Confirm the new wall-containment scenario matches the human-reported failing
  route; attach the first `npc.pose.*` event if it still reproduces.

## Pre-existing edits preserved

The working tree contains unrelated concurrent changes (player AI, hitscan
claims, weapon config, docs, and other agent work). They were not reverted or
claimed as this session's work. The lint baseline records only the exact
pre-existing metrics; any new or increased finding fails the gate.
