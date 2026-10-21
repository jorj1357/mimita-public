# Juggernaut presentation wiring

- Status: `SOURCE_UPDATED_BUILD_FAILED_RUNTIME_UNVERIFIED`
- Date and time: 2026-10-21T03:20:54Z (2026-10-20 23:20:54 EDT)
- Branch: `20261011zt`
- Base commit: `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- Final commit: unchanged; no commit created.

## Scope and preserved work

The checkout already contained unrelated edits in configuration, gameplay,
other mode specifications, and changelogs. Those edits were preserved. The
requested Juggernaut values were already present in
`config/gamemodes/juggernaut.json`; that file was not changed.

## Source changes

- Routed explicit mode presentation overrides after role and weapon preset
  application for damage numbers, hit effects, world impacts, blood, hit
  markers, and hit sounds. Nested `blood` and `ragdolls` also resolve to the
  existing networked root controls.
- Applied `enemy_healthbars` to live enemy nameplates while preserving
  `hide_healthbars` as the all-healthbar switch. Applied the `killfeed` switch
  to incoming/live killfeed events and display.
- Applied `player_outlines` and team colors in the renderer. Unknown team
  identity waits for authoritative team data instead of falling back to stale
  per-player data. The mode switch controls whether remote players are outlined.
- Added a structured profile-change event,
  `client.gamemode.presentation.applied`, for source-level inspection.
- Updated the graphics presentation spec to describe the implementation and
  keep build/runtime acceptance explicitly open. Updated the Juggernaut note
  to use its requested friendly/enemy team colors.

## Validation and acceptance

- `git diff --check` reported trailing whitespace in the user-added note at
  lines 7, 9, and 12 of `docs/specs/gamemodes/juggernaut.md`; it reported no
  other whitespace errors in this change set.
- The canonical dev-loop build request was accepted but failed after about 70
  seconds while updating `.dev/current-run.json`: Windows returned
  `WinError 5: Access is denied` replacing the status file. Dev-loop status
  still identifies build 2722 as the latest successful executable; that build
  predates these source changes, so it is not validation of this patch.
- An existing `npc-navigation` dev-loop run had a separate Melee client
  active. It was not stopped or changed. No Juggernaut runtime scenario,
  live journal inspection, or human visual acceptance was performed.
- No tests were added or run.

## Changed source and documentation files

`src/combat/actor-preset-weapons.cpp`,
`src/combat/actor-preset-weapons.h`, `src/effects/hit-effects.cpp`,
`src/engine/engine-tick-render.cpp`, `src/engine/engine-tick-ui-game-hud.cpp`,
`src/gamemode/gamemode.cpp`, `src/gui/hud/healthbar-config.cpp`,
`src/gui/hud/healthbar-config.h`, `src/gui/hud/player-nameplates.cpp`,
`src/gui/hud/player-nameplates.h`, `src/killfeed/killfeed.cpp`,
`src/killfeed/killfeed.h`, `src/network/community-match-client.cpp`,
`src/network/community-match-client.h`,
`src/network/confirmed-damage-presentation.cpp`, `src/render/render-player.cpp`,
`docs/specs/graphics/graphics-presentation.md`, and
`docs/specs/gamemodes/juggernaut.md`.
