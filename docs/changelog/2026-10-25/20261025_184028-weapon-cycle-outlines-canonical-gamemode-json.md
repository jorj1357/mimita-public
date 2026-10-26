# Weapon cycle, server-path outlines/trails, and canonical gamemode JSON

- Result: PASS_WITH_HUMAN_REVIEW.
- Timestamp: 2026-10-25T18:40:28Z.
- Branch: `main`.
- HEAD observed: `836330ef`.
- Latest verified build: `3355`.
- Runtime verification build: `3349`.
- Skills read: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`.
- Documents read: `docs/ROUTER.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/architecture/player-npc-systems/npc-movement.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/open-gameplay-issues.md` (line-180 handoff),
  `docs/regressions/README.md`.

## Scope

This session stayed on the authoritative server → client path. No local/offline
Duel implementation was added or expanded. Existing concurrent work by another
contributor (`spawn-utils`, map/collision files, and the commit at HEAD) was
preserved and not claimed as this session's work.

## What changed

### Server-path player outlines

- `src/render/render-player.cpp`: the mode-forced outline no longer passes
  `0.0f` world thickness into the shader. It now renders an expanded back-face
  shell using `uOutlinePass` + `uOutlineThickness`, with a bounded world
  thickness derived from the authored `thickness`.
- `src/render/render-player.cpp`: added a rate-limited
  `player_visuals.outline` event recording actor team, local team, team
  identity, model readiness, thickness, alpha, mode-forced state, drawn/skipped,
  and the exact reason.
- `config/gamemodes/masterlist/gamemode-masterlist.json` and every
  `config/gamemodes/*.json`: mode presentation now explicitly enables
  `player_outlines` and `friendly_outlines`, sets `enemy_outline_alpha` to 1.0,
  and supplies team outline colors. This makes outlines visible in every mode
  instead of inheriting the local `enemy.outline.alpha=0.0`.
- The evidence is in the runtime journal: `player_visuals.outline` reports
  `drawn=true`, `mode_forced=true`, `team_identity_known=true`,
  `thickness=2.0`, `world_thickness~0.03`, `alpha=1.0`.

### Trails

- `src/render/render-player.cpp`: added `player_visuals.trail` diagnostics with
  actor, enabled state, speed, min speed, sample count, width, alpha, lifetime,
  and drawn/skip reason. The emitted state is rate-limited so normal
  collect/draw oscillation does not flood the journal.
- Added mode-level trail overrides:
  `presentation.enemy_trail` and `presentation.teammate_trail`, each with
  `enabled`, `minSpeed`, `sampleIntervalSeconds`, `lifetimeSeconds`,
  `maxSamples`, `alpha`, `width`, and `color`.
- `GamemodeTrailSettings` and `GamemodePresentation` now carry trail policy.
- `PlayerVisualsConfig` now owns effective enemy/teammate trail overrides and
  render reads the effective trail settings without rewriting
  `config/playervisuals.json`.
- `CommunityMatchClient::onState` applies mode trail overrides on the
  server/client path and includes them in the presentation fingerprint.
- Invalid `playervisuals.json` now produces an in-game `CONFIG ERROR` tip naming
  the file and exact validation error, in addition to the journal event.
- Runtime evidence: `player_visuals.trail` reported `state=drawn`, enabled,
  nonzero points, width 0.55, alpha 0.9.

### NPC weapon visit cycle

- `src/npc/npc-behavior.h/.cpp`: added `weapon_cycle`, `weapon_visit_shots`,
  `weapon_visit_jitter_shots`, `weapon_switch_delay_min/max`, and
  `weapon_cycle_include_melee` to behavior profiles and the elite skill tier.
- `config/behavior-profiles.json`: `elite_squad` now authors:
  - shotgun 2 accepted attacks,
  - revolver 6,
  - rocket launcher 1,
  - projectile rifle 2,
  - hitscan rifle 6,
  - big shotgun 1,
  - large machine gun 12,
  - then cycles again as weapons finish reloading.
- `src/npc/npc.cpp`: added per-NPC cycle state, cycle start/visit completion,
  immediate next-tick switching, skipped unusable/non-attack weapons, randomized
  switch delay, and `npc.weapon-cycle` evidence with previous/next weapon and
  visit counts.
- `src/npc/npc-combat.cpp`: only accepted attacks decrement a visit.
- `config/weaponsets.json`: added `elite_arsenal` containing every attack weapon
  available to Duel/TDM NPCs.
- `config/roles.json`: added `elite_npc` using `elite_arsenal` and
  `elite_squad`.
- Duel/TDM NPC slots now use `elite_npc`; human slots keep `elite_fighter`.
- Runtime evidence from TDM:
  - `npc.weapon-cycle` shows `revolver -> shotgun` (`visit_shots_total=2`),
    `shotgun -> revolver` (`visit_shots_total=6`),
    `revolver -> rocket_launcher` (`visit_shots_total=1`),
    `rocket_launcher -> projectile_rifle` (`visit_shots_total=2`), and more.
  - Client `SHOT_EVENT_RECV` shows multiple distinct weapon types, confirming
    the cycle actually fires the changed weapons.
- Melee-class items that the shared NPC attack path cannot execute are skipped
  with a cycle event reason rather than consuming a visit.

### Canonical gamemode JSON

- `devscripts/gamemode_json_sync.py`: new canonicalizer/checker.
  - `--check` reports missing, unknown, and out-of-order settings.
  - `--write` fills every masterlist field, preserves all existing values, uses
    `null` for inherit-by-default presentation/trail values, and sorts fields in
    exact masterlist order.
- `config/gamemodes/masterlist/gamemode-masterlist.json`: added explicit
  `enabled` policy switches, nullable presentation/trail fields, missing string
  defaults, and the full trail schema.
- All `config/gamemodes/*.json` are now complete and canonical in masterlist
  order. A value-preservation audit against HEAD found only the intended
  changes:
  - Duel/TDM `npc_role` changed from `elite_fighter` to `elite_npc`.
  - TDM `enemy_outline_alpha` changed 0.01 -> 1.0.
  - Juggernaut `enemy_outline_alpha` changed 0.25 -> 1.0.
  - All other pre-existing values were preserved.
- `config_selftest.py` now runs the canonical check and gamemode schema
  validation.
- Runtime loader now honors `enabled:false` for `payload`, `npc_targeting`,
  `npc_travel`, and `avatar`; accepts nullable presentation/trail values; and
  treats null `ragdoll_enabled`, `blood_enabled`, and `one_rotator` as
  “no override.”

## Verification

### Build and selftests

- Build `3352`: SUCCESS.
- `.dev/builds/3352/mimita.exe --npc-behavior-profile-selftest`: PASS,
  including elite weapon visits and switch-delay checks.
- `--team-brain-selftest`: PASS.
- `--gamemode-selftest`: PASS; TDM resolves `elite_squad`, `elite_fighter`,
  `elite_npc`, and `team_roster`.
- `--gamemode-config-validate`: PASS, `masterlist=1 loaded=16 errors=0`.
- `python devscripts/gamemode_json_sync.py --check`: PASS.
- `python devscripts/config_selftest.py`: gamemode subset reports 16 modes,
  0 gamemode errors. The overall command still fails on pre-existing
  avatar-folder validation issues unrelated to this change.

### Runtime

- Journal: `logs/2026-10-25/20261025_222756/events.jsonl`
  (shared dev-loop run; runtime build `3349` for the visual/cycle evidence).
- `client.gamemode.presentation.applied` for TDM reports `player_outlines=true`,
  `friendly_outlines=true`, `enemy_outline_alpha=1.0`,
  `enemy_outline_color=[255,0,0]`.
- `player_visuals.outline` reports `drawn=true`, `mode_forced=true`,
  `team_identity_known=true`, `thickness=2.0`, `world_thickness~0.03`,
  `alpha=1.0`.
- `player_visuals.trail` reports `state=drawn`, enabled true, nonzero points,
  width 0.55, alpha 0.9.
- `npc.weapon-cycle` reports the shotgun/revolver/rocket/rifle visits and
  counts.
- Client `SHOT_EVENT_RECV` shows multiple weapon type values, confirming the
  cycle reaches actual firing.

## Remaining human review

- Visually confirm the outline shell thickness and trail appearance in the live
  game window. The journal proves the draw path executed, not visual acceptance.
- Confirm the weapon cycle feels appropriately fast and human, and whether the
  switch delays should be more randomized.
- Decide whether every mode should force outlines at alpha 1.0 permanently, or
  only the modes where visibility is desired.
- Confirm whether SpyKnife/force-punch should eventually participate in the
  rapid weapon cycle once the shared NPC melee action path is wired.

## Follow-up: enforced completeness and JSON contract documentation

- `docs/architecture/json-configuration/json-configuration.md`: added the
  “Gamemode masterlist and canonical mode files” contract. It documents the
  complete-file/order rule, `null` inherit semantics, `enabled:false` optional
  blocks, `--check`/`--write`, runtime validation, last-known-good reload,
  diagnostics/tips, behavior-preserving defaults, how to add a setting, and
  which owners are involved.
- `src/gamemode/gamemode.cpp`: missing nested fields are now rejected, not only
  missing top-level fields. Every masterlist field is required for a complete
  mode file, so a mode can no longer load with a silently absent optional field.
- `devscripts/config_selftest.py`: the offline schema check also requires every
  nested masterlist field.
- Build `3355`: SUCCESS.
- Tampered-file proof: a temporary copy of `tdm.json` with
  `presentation.enemy_outline_alpha` removed produced
  `tdm.json -> presentation.enemy_outline_alpha: missing setting
  (all masterlist fields are required)`, `loaded=0`, exit 1.
- Real mode files: `--gamemode-config-validate` still reports
  `masterlist=1 loaded=16 errors=0`.
