# NPC navigation source inventory

These repositories are present for investigation and isolated integration
experiments. They are not wired into the MiMITA build or runtime yet.

## Recast Navigation

- Local path: `external/recastnavigation/`
- Upstream: https://github.com/recastnavigation/recastnavigation
- Pinned shallow-clone commit: `9f4ce64`
- Relevant modules:
  - `Recast/` — navigation-mesh generation
  - `Detour/` — runtime queries and path corridors
  - `DetourTileCache/` — tiled rebuilds and temporary obstacles
  - `DetourCrowd/` — optional later local crowd avoidance
  - `DebugUtils/` — navigation debug drawing helpers
- License: `external/recastnavigation/License.txt`
- MiMITA role: preferred global navigation backend candidate.

## RVO2 / ORCA

- Local path: `external/rvo2/`
- Upstream: https://github.com/snape/RVO2
- Pinned shallow-clone commit: `75822bb`
- License: `external/rvo2/LICENSE`
- MiMITA role: optional later local agent-avoidance comparison; not a global
  navigation replacement and not an actor-physics owner.

## MicroPather

- Local path: `external/micropather/`
- Upstream: https://github.com/leethomason/MicroPather
- Pinned shallow-clone commit: `33a3b84`
- License information: `external/micropather/readme.md`
- MiMITA role: small generic A* comparison candidate if a standalone graph
  solver is useful; not a collision-to-navmesh solution.

## Integration rule

Do not include these repositories in the production build until the plan in
`docs/specs/20261006plan.md` reaches the isolated experiment and dependency
review gates. Preserve upstream license and attribution files if source is
copied, vendored, or modified.
