# Live mode status and NPC avatar policy intake

- Recorded: 2026-10-26T13:43:26Z (2026-10-26 09:43:26 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request

Record the report that dev-loop continues to display an old Duel launch mode
while the player is in TDM, and explain the design needed for mode-selected NPC
avatar groups and per-NPC identity across respawns. TDM should keep each NPC's
own selected avatar through repeated deaths; other modes should be able to
choose whether to reroll.

## Documentation changes

- `docs/specs/open-gameplay-issues.md`:
  - New intake lines 19-25 records the 2026-10-26 09:39 EDT dev-loop mode
    report. It notes that no status or journal snapshot after the claimed TDM
    switch was captured and requests that launch mode and live mode be
    distinguished.
  - New intake lines 26-35 records the source finding and requested
    mode-selectable NPC avatar group and per-NPC respawn identity policy.
  - Issue table line 469 records the dev-loop status problem.
  - Architecture-planning table line 807 describes the requested shared group,
    stable-per-NPC versus reroll-per-life setting, authoritative avatar
    selection, and the intended TDM behavior.
- `docs/specs/open-gameplay-issues-human.md` mirrors those additions at lines
  27-43, 279, and 617.

## Source findings and proposed design explanation

- `src/npc/npc-avatar.cpp::chooseAvatar()` refreshes its choices from
  `AvatarSystem::listAvatars()` and selects from the entire list. That list in
  `src/avatar/avatar.cpp::AvatarSystem::listAvatars()` includes every valid
  avatar directory; this selection path does not call the mode avatar policy.
- NPC selection seeds from both the NPC ID and `transformEpoch`, so a new life
  can choose another avatar. Server profile resolution carries mode
  allow/deny rules, but NPC spawn adoption only replaces the random avatar when
  the resolved profile has a nonempty forced avatar name.
- `GamemodeRegistry::loadMasterlist()` is called by `loadDirectory()`;
  `GamemodeRegistry::pollReload()` watches mode JSON files but not the
  masterlist. Thus, a running process can reload a TDM file against its old
  cached masterlist enum. A restart reloads the schema; it does not fix the
  separate NPC random-selection bypass.
- A clean design is a mode-level `npc_avatar` policy that reuses the
  masterlist's named `avatar_groups`, plus a mode-level lifecycle setting such
  as `stable_per_actor` or `reroll_per_life`. The server should select an
  eligible avatar once, retain it on the stable NPC/roster identity across
  respawn, and replicate the selected name. TDM would choose
  `competitive_no_headless` and `stable_per_actor`; another mode could select
  the same group and `reroll_per_life`. This interprets “one avatar forced” as
  one stable selection per NPC, not one shared avatar for every NPC.
- The current working-tree masterlist also contains an extra `*girl*` deny
  entry under `competitive_no_headless`; that excludes girl-named avatars in
  addition to the requested monster/headless exclusions. It was preserved.
- The dev-loop status inspected during this turn identified run
  `20261026_033620`, launch mode `duel_presentation`, and server arguments
  `--gamemode duel`. This is consistent with the older launch profile reported
  by the player, but the status check preceded the user's 09:39 EDT report and
  does not prove what the server was running after the claimed switch to TDM.

## Review and validation

- Consulted `docs/ROUTER.md`,
  `docs/specs/actor-presets/actor-presets.md`,
  `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/gamemodes/teamdeathmatch.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/specs/avatar/avatar-editor.md`,
  `docs/specs/dev-loop/dev-loop.md`,
  `docs/architecture/json-configuration/json-configuration.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/documentation-checker-v1.md`, and
  `docs/operations/task-completion/task-completion.md`.
- The router-referenced `docs/doc-review-09-03-2026.md` and
  `docs/regressions/regressions-v1.md` are absent from this checkout.
- `git diff --check`: `PASS`.
- No game code or runtime configuration was changed. No build or runtime test
  was run. The dev-loop mode report and NPC avatar behavior remain unverified
  after a TDM mode switch.

## Existing working-tree edits preserved

- Preserved modifications in `config/accounts/default.json`,
  `config/gamemodes/masterlist/gamemode-masterlist.json`,
  `config/gamemodes/tdm.json`, `config/gui/hud.json`, `config/movement.json`,
  `docs/specs/open-gameplay-issues.md`, and
  `docs/specs/open-gameplay-issues-human.md`.
- In particular, did not alter the working-tree TDM goal/capacity changes, its
  weapon set 29, `avatar.forced: true` with an empty `name`, or the current
  `competitive_no_headless` group contents.
- Left the prior untracked changelogs untouched. No files were committed.

## Human review still needed

Confirm the desired wording/labels for launch mode versus live game mode. When
the NPC avatar policy is implemented, test TDM with multiple NPCs through
several deaths and verify each keeps its own eligible avatar; test a mode
configured to reroll so the per-mode choice is demonstrated separately.
