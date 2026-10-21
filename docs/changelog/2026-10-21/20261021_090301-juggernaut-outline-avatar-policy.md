# Juggernaut outline and avatar policy

- Timestamp: 2026-10-21T09:03:01Z (America/New_York display date 2026-10-21)
- Branch: `20261011zt`
- HEAD: `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope and pre-existing work

Added the requested open gameplay issue stating that old local/offline Duel gameplay paths should be removed and gameplay modes must use client/server behavior. Made Juggernaut's existing mode presentation section own outline permission, friendly outline visibility, and hostile outline opacity. Added role-preset avatar allowlists containing the named boy/girl body variants, excluding monster and oversized avatar folders.

The worktree was already dirty before this task, with many unrelated modified and untracked files (including existing Duel, Melee, presentation, and logging changes). Those changes were preserved. No commit was created. The dev-loop was already running build 2733 in Duel mode on map `xxity2`; it was left running and not restarted.

## Exact edits

1. `src/gamemode/gamemode.h:139-140`
   - Before: `bool hasPlayerOutlines = false;      bool playerOutlines = true;`
   - After: added `hasFriendlyOutlines` / `friendlyOutlines` and `hasEnemyOutlineAlpha` / `enemyOutlineAlpha` fields beside the player outline fields.
   - The nearby contract comment now says outline permission is opt-in during community matches.
2. `src/gamemode/gamemode.cpp:302-305`
   - Before: only `player_outlines` was parsed in the outline portion of `presentation`.
   - After: parses `friendly_outlines` and clamps numeric `enemy_outline_alpha` to `[0.0, 1.0]`.
3. `src/combat/actor-preset-weapons.cpp:41-42`
   - Before: mode presentation equality compared the player outline flag and team colors.
   - After: equality also compares friendly-outline and hostile-alpha fields so profile changes are noticed.
4. `src/config/player-visuals-config.h:80-85`
   - Before: exposed mode player-outline and team-color overrides only.
   - After: exposes friendly-outline and hostile-alpha accessors; stores the temporary mode policy in the config owner.
5. `src/config/player-visuals-config.cpp:151-160`
   - Before: no mode-owned friendly visibility or hostile alpha operations.
   - After: stores friendly visibility and a clamped hostile alpha override; can clear/query the alpha override.
6. `src/network/community-match-client.cpp:415-433,689-690`
   - Before: if `player_outlines` was missing, it cleared the mode override and kept local preferences.
   - After: every active community mode applies `player_outlines` as an explicit opt-in (`missing` is off), and applies friendly visibility and hostile alpha from the same profile. The profile-change journal includes both added values. Reset clears the hostile-alpha override.
7. `src/render/render-player.cpp:212-243`
   - Before: the renderer used local per-team outline mode and enabled outlines for configured teammates when the mode enabled outlines.
   - After: mode-disabled outlines are suppressed including the local actor; known enemies can be outlined when the mode permits; friendly/local outlines require `friendly_outlines=true`; unknown-team actors remain suppressed. Hostile alpha and team color are applied to enemy outlines.
8. `config/gamemodes/juggernaut.json:71-74`
   - Before: `player_outlines: true` plus friendly green and enemy red colors, which let the renderer use the user's teammate outline preference.
   - After: `player_outlines: true`, `friendly_outlines: false`, enemy red, and `enemy_outline_alpha: 0.25`.
9. `config/actor-presets/juggernaut_fighter.json:14-23` and `config/actor-presets/juggernaut_arcade.json:12-21`
   - Before: each role preset had no avatar allowlist.
   - After: each has `avatar.forced: false` and an editable `avatar.allowed` list of the selected normal boy/girl body IDs. Oversized and monster IDs are absent. Existing actor-preset selection enforcement handles rejected selections/manifests.
10. `docs/specs/gamemodes/juggernaut.md:21-31,59-60`
    - Before: avatar groups were unresolved and the design asked for friendly and enemy outlines.
    - After: records the shared editable role-preset human avatar allowlists, no friendly outline by default, and mode-owned enemy color/alpha.
11. `docs/specs/graphics/graphics-presentation.md:28-32,48-52,144-151`
    - Before: described every omitted mode value as inheriting user settings.
    - After: documents `player_outlines` as an explicit community-match opt-in, `friendly_outlines`, the bounded alpha setting, and match exit restoration.
12. `docs/specs/open-gameplay-issues.md:88`
    - New row (no prior row): `Legacy Duel cleanup` records removal of local/offline Duel paths, client/server-only gameplay, and the need to audit callers before deletion.

## Documents and focused review

Read `docs/ROUTER.md`, the relevant gamemode/actor-preset/graphics specifications, JSON configuration architecture, runtime scenario workflow, build/EXE instructions, task completion instructions, and `docs/skills/documentation-checker-v1.md`. `docs/doc-review-09-03-2026.md`, which the router names, is absent in this checkout.

The routed TODO scan found unrelated open notes and did not edit them:
- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:97`: `TODO: find a better long-term aiming solution`. Suggested expansion: define the desired stable arm/crosshair behavior, owner, and live acceptance scenario.
- `docs/specs/gamemodes/juggernaut-duel.md:226`: `Networking TODO: give role-based score-limit modes one consistent weapon-set`. Suggested expansion: specify the role/weapon-set replication contract and the expected server/client acceptance evidence.

## Validation and evidence

- `config/gamemodes/juggernaut.json` and both actor-preset files parsed as JSON/JSONC; both 17-avatar lists resolve to existing `assets/avatars/` folders.
- Canonical command `python devscripts/dev-loop.py --request build --wait-for-build`: success, build 2737, current source generation 3; 19 objects compiled, 545 skipped.
- `python devscripts/dev-loop.py --status --json`: reports `published_current`, build 2737, source generation 3. The active client/server are still build 2733, launch mode Duel; they were not interrupted.
- `.dev/builds/2737/mimita.exe --versioninfo`: executable identity printed; `EVENTS_JSONL_PATH=logs/10-21-2026/20261021_050251/events.jsonl`.
- No live Juggernaut gameplay scenario or visual review was run. The version-info process is not gameplay evidence. Human confirmation is still needed for no teammate outlines, faint red hostile outlines, avatar rejection/selection, and damage-number visibility in Juggernaut. No tests were added or run.

## Open boundary

The old offline/local Duel implementation was recorded in the inbox, as requested; it was not deleted. Its call sites and safe removal boundary still need an audit. To verify Juggernaut live, restart/launch the dev-loop session into Juggernaut with a new executable and inspect that run's journal and visuals. This session's existing running Duel session was preserved.

## Verbatim edit excerpts

The excerpts below are the exact changed contract lines. For additive code/config fields, “before” means there was no corresponding mode-owned field or policy.

- `src/gamemode/gamemode.h:139-140`
  - Before: `bool hasPlayerOutlines = false;      bool playerOutlines = true;`
  - After: `bool hasFriendlyOutlines = false;     bool friendlyOutlines = false;` and `bool hasEnemyOutlineAlpha = false;   float enemyOutlineAlpha = 0.25f;`
- `src/gamemode/gamemode.cpp:302-305`
  - Before: `readFlag("player_outlines", next.presentation.hasPlayerOutlines, next.presentation.playerOutlines);`
  - After: `readFlag("friendly_outlines", next.presentation.hasFriendlyOutlines, next.presentation.friendlyOutlines);` plus numeric `enemy_outline_alpha` parsing clamped with `std::clamp(..., 0.0f, 1.0f)`.
- `src/combat/actor-preset-weapons.cpp:41-42`
  - Before: `a.hasPlayerOutlines == b.hasPlayerOutlines && a.playerOutlines == b.playerOutlines &&`
  - After: `a.hasFriendlyOutlines == b.hasFriendlyOutlines && a.friendlyOutlines == b.friendlyOutlines &&` and `a.hasEnemyOutlineAlpha == b.hasEnemyOutlineAlpha && a.enemyOutlineAlpha == b.enemyOutlineAlpha &&`
- `src/config/player-visuals-config.h:80-85`
  - Before: `glm::vec3 effectiveOutlineColor(bool teammate, const glm::vec3& configured) const;`
  - After: `void setFriendlyOutlinesEnabled(bool enabled);` and the `setEnemyOutlineAlpha`, `clearEnemyOutlineAlphaOverride`, and `effectiveEnemyOutlineAlpha` declarations.
- `src/config/player-visuals-config.cpp:151-160`
  - Before: no friendly-visibility or mode-alpha methods.
  - After: `void PlayerVisualsConfig::setFriendlyOutlinesEnabled(bool enabled) { mFriendlyOutlinesEnabled = enabled; }` and `mEnemyOutlineAlpha = std::clamp(alpha, 0.0f, 1.0f);` with matching clear/get methods.
- `src/network/community-match-client.cpp:415-433`
  - Before: `else PlayerVisualsConfig::instance().clearPlayerOutlinesOverride();` when the mode omitted `player_outlines`.
  - After: `PlayerVisualsConfig::instance().setPlayerOutlinesEnabled(modeConfig.presentation.hasPlayerOutlines && modeConfig.presentation.playerOutlines);`; friendly visibility and hostile alpha are set from the same presentation object.
- `src/render/render-player.cpp:212-243`
  - Before: mode-enabled outlines used the locally configured teammate/enemy modes and `outline->alpha`.
  - After: `mode = teamIdentityKnown && (!teammate || visualsConfig.friendlyOutlinesEnabled()) ? "outline" : "none";`; hostile alpha comes from `effectiveEnemyOutlineAlpha` when the mode supplies it.
- `config/gamemodes/juggernaut.json:71-74`
  - Before: `"friendly_outline_color": [0, 255, 0],` and `"enemy_outline_color": [255, 0, 0]`
  - After: `"friendly_outlines": false,`, `"enemy_outline_color": [255, 0, 0],`, `"enemy_outline_alpha": 0.25`
- `config/actor-presets/juggernaut_fighter.json:14-23` and `config/actor-presets/juggernaut_arcade.json:12-21`
  - Before: no `avatar` object.
  - After: `"avatar": { "forced": false, "allowed": ["abusiveboy", "abusivegirl", "abusivegirl2", "abusivegirl3", "abusivegirlheadless", "abusivegirlheadless2", "abusivegirlheadless3", "abusivegirlheadless4", "boiledboy", "headless", "headlessgirl", "headlessgirl2", "headlessgirl3", "headlessredux", "headlessredux2", "jason", "jasonheadless"] }` (formatted across lines in each file).
- `docs/specs/gamemodes/juggernaut.md:21-31`
  - Before: `Fighters use an approved human-sized avatar group; Juggernauts use an approved large-body avatar group.`
  - After: `Both roles use the editable avatar.allowed lists in their actor-preset JSON.` and `friendly outlines follow friendly_outlines (false by default)`.
- `docs/specs/graphics/graphics-presentation.md:28-32,48-52,144-151`
  - Before: `A mode may omit any setting to inherit the normal actor or user configuration.`
  - After: `player_outlines is an opt-in exception for community matches: outlines are off unless the active mode explicitly sets it to true.` The table now defines `friendly_outlines` and bounded `enemy_outline_alpha`.
- `docs/specs/open-gameplay-issues.md:88`
  - Before: no Legacy Duel cleanup entry.
  - After: `| Legacy Duel cleanup | Remove old offline/local Duel gameplay paths. Duel and other gameplay modes must be client/server; there is no supported or maintained offline gameplay implementation. ... |`

