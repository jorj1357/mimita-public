# Destructible world disabled by default; retrospective and re-enable specs

Date: `2026-10-31T15:20:55Z`

## Summary

World-geometry destruction is now disabled by default through JSON. Static
authored map geometry is the default again. The shared impact/map-destruction
code and diagnostics are intentionally kept as a future starting point.

The switch affects **map geometry only**. Crates, props, vehicles, actor contact,
and ordinary collision keep their existing behavior.

This is a design/priority decision: the feature worked technically, but
destroying floors and slopes made TDM's walkable world inconsistent and less
fun. The lesson is recorded in the gameplay issue intake and a gold
retrospective.

## Configuration

- `config/destructible-world.json`
  - `world.enabled: false`
  - comment records the 2026-10-31 decision and points to the spec section.
  - `fragments.fragmentInstantDeleteMaxDimMeters` remains `0.15`; the desired
    re-enable value of `0.1` is documented, not applied.
- `config/gamemodes/tdm.json`
- `config/gamemodes/juggernaut.json`
- `config/gamemodes/jugger_horde.json`
  - `features.destructible_world: null` so they inherit the global `false`.
- `config/materials.json` foundation remains finite but extreme:
  `cutThresholdJ: 1.0e16`. The value is documented in the spec and kept.

## Code

- `src/network/server-world-destruction.cpp`
  - the dedicated server now loads and hot-polls
    `DestructibleWorldConfig` before reading `world.enabled`.
  - Previously the server could keep the built-in default (`true`) because the
    client-only config polling path did not run on the dedicated server.

No destruction code was removed. The shared impact entry point, map activation,
fragment/replication path, and diagnostics remain available for a future
re-enable.

## Documentation

- `docs/specs/destructible-world/destructible-world.md`
  - section 1 now records the current disabled default and map-only scope.
  - new section 72 records:
    - the decision and why it was disabled,
    - what worked,
    - Map Editor material/walkability assignment work,
    - floor/slope classification using the shared `walkableSlopeDot`,
    - foundation value and current material-formula truth,
    - small-fragment threshold,
    - missing NPC hitscan world impacts,
    - NPC hole/navigation refresh,
    - unsupported-piece falling,
    - performance/re-enable conditions.
- `docs/specs/gamemodes/teamdeathmatch.md`
- `docs/specs/gamemodes/juggernaut.md`
- `docs/specs/gamemodes/survive-the-disasters.md`
- `docs/specs/gamemodes/gamemodes.md`
  - current disabled status and mode inheritance.
- `docs/specs/entity-editor/entity-editor.md`
  - corrected the GLB material section to future/data-layer status and added
    walkable/floor/slope selection requirements.
- `docs/specs/weapons/weapons.md`
  - world-impact/explosion profiles remain the re-enable contract; map
    destruction is currently off.
- `docs/specs/networking/networking.md`
  - world-destruction authority is currently dormant for map geometry.
- `docs/specs/open-gameplay-issues.md`
  - new intake records the decision, the lesson, known gaps, and the
    not-a-priority status.
- `docs/gold/2026-10-31-destructible-world-retrospective.md`
  - gold-quality retrospective with evidence links, root-cause table, lesson,
    decision, and re-enable gate.

## Validation

- Debug build `3993` succeeded through the dev loop.
- `mimita.exe --destructible-selftest`: PASS.
- `mimita.exe --destruction-stress-selftest`: PASS.
- `python devscripts/gamemode_json_sync.py --check`: PASS.
- JSONC parse checks for `config/destructible-world.json`,
  `config/materials.json`, and the three changed mode files: OK.
- Runtime TDM + Dust2 with the new build:
  `C:\mimita-v9\logs\2026-10-31\20261031_151945\events.jsonl`
  - map loaded (`collision_triangles=5469`, `spawn_points=2`),
  - TDM reached active phase,
  - **zero `world.destructible*` events** after the switch was applied.

This verifies the runtime disable path on the exact executable; human gameplay
acceptance of the static TDM world remains a separate check.

## Not changed

- No map geometry, material values, weapon profiles, or fragment thresholds were
  deleted.
- No Map Editor UI was added in this slice; the future requirements are
  documented.
- No NPC hitscan or NPC hole-navigation fix was added; the gaps are documented
  for a future re-enable.
- Crates/props remain independently destructible.
