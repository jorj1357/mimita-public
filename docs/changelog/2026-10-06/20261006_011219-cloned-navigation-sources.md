# Clone NPC navigation source repositories

Date (UTC): 2026-10-06T05:12:19Z
Display timezone: America/New_York
Display time: 2026-10-06 01:12:19 EDT
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Cloned the requested open-source navigation candidates into the repository for
offline inspection and future isolated experiments. No production build or
runtime integration was performed.

## Added paths

- `external/recastnavigation/` from
  `https://github.com/recastnavigation/recastnavigation.git`, commit `9f4ce64`.
- `external/rvo2/` from `https://github.com/snape/RVO2.git`, commit `75822bb`.
- `external/micropather/` from
  `https://github.com/leethomason/MicroPather.git`, commit `33a3b84`.
- `external/npc-navigation-sources.md`, an inventory with links, commits,
  roles, and license locations.

All three source repositories were cloned with shallow history (`--depth 1`).

## License evidence

- Recast/Detour license preserved at `external/recastnavigation/License.txt`.
- RVO2 Apache license preserved at `external/rvo2/LICENSE`.
- MicroPather license information is recorded in its upstream
  `external/micropather/readme.md`.

## Scope boundary

The repositories are not referenced by MiMITA build files, C++ source, or
runtime configuration. Recast/Detour remains the preferred global-navigation
candidate; RVO2 remains an optional local-avoidance candidate; MicroPather
remains an A* comparison candidate.

## Validation

- Verified each upstream remote and pinned commit.
- Verified expected top-level source directories and license files.
- Searched MiMITA source/build files and found no integration references.
- Preserved all pre-existing worktree changes.

Human review is still needed before adding any of these sources to the build,
including dependency pinning, source-vendoring policy, and license-attribution
review.
