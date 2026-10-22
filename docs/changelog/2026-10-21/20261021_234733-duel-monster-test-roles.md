# Duel Monster Test Roles

- Result: PASS_WITH_HUMAN_REVIEW
- Time: `2026-10-21T23:47:33Z` (Eastern display: `2026-10-21 19:47:33 EDT`)
- Branch: `20261011zt`
- HEAD at start: `b29ab344`
- Scope: Configure Duel player and NPC roles to force the existing `monstertest1` avatar.

## Existing workspace changes

The following changes were present before this task and were preserved:

- Modified: `config/analytics.json`, `config/animations.json`, `config/audio/music-settings.json`, `config/movement/movement-source.json`, `config/playervisuals.json`, `config/ragdoll.json`, `devscripts/dev-loop.py`, and `docs/specs/movement/movement.md`.
- Untracked: `assets/maps/payload1.glb`, `docs/changelog/2026-10-21/20261021_232030-dev-loop-start-recovery.md`, `docs/regressions/2026-10-21/dev-loop-status-write-race-REG.md`, and `docs/specs/fortnite/`.

## Changes

### `config/roles.json`

- Lines 319-362 replace the four old Duel role definitions with four role IDs: `duel_monster_test_red`, `duel_monster_test_blue`, `duel_monster_test_red_npc`, and `duel_monster_test_blue_npc`.
- Each role keeps its RED or BLUE team identity and `duel_standard` preset. Each sets `avatar` to `monstertest1` and `avatar_forced` to `true`. NPC roles retain `behavior_profile: balanced`.
- Before: the role IDs were `duel_red`, `duel_blue`, `duel_red_npc`, and `duel_blue_npc`; none selected a forced avatar.
- After: the four role IDs are the `duel_monster_test_*` roles above, each with `"avatar": "monstertest1"` and `"avatar_forced": true`.

### `config/gamemodes/duel.json`

- Lines 36-56 update the role-count map and both team role references.
- Before: RED used `duel_red` / `duel_red_npc`; BLUE used `duel_blue` / `duel_blue_npc`.
- After: RED uses `duel_monster_test_red` / `duel_monster_test_red_npc`; BLUE uses `duel_monster_test_blue` / `duel_monster_test_blue_npc`. The role-count map now names the two player roles.

### `config/actor-presets/duel_standard.json`

- Lines 11-16 make the shared preset force and allow only `monstertest1`.
- Before: `"forced": false`, `"name": "jason"`, `"allowed": []`, and `"denied": ["monster*"]`.
- After: `"forced": true`, `"name": "monstertest1"`, `"allowed": ["monstertest1"]`, and `"denied": []`.

### `docs/specs/gamemodes/duel.md`

- Lines 24-26, 58-60, 99-105, 118, and 154-155 now specify that both player and NPC roles force `monstertest1`, superseding the prior human avatar choice and `monster*` denial rule.
- Before: human avatars were user-selected and monster IDs were denied.
- After: every Duel actor uses the forced, allow-listed `monstertest1`; player/NPC role IDs are listed explicitly and preserve each team's identity.

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/gamemodes/duel.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, and `docs/skills/spec-behavior-review-v1.md`.
- Spec review: the requested forced test avatar superseded Duel's earlier stated human avatar-selection rule. The Duel spec was updated to match the user's explicit change.
- Validation: all three changed JSON files parsed; all four role references resolved; each role's avatar, forced flag, and team ID matched its team; the actor preset forces and allows `monstertest1` with no deny patterns.
- No build or game runtime scenario was run. The edit changes configuration only; a live Duel still needs human review to confirm avatar appearance and application in game.
- No regression was confirmed, so no regression record was added.
