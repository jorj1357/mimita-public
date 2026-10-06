# Dust 2 map-load evidence phase

- Status: `PASS_WITH_HUMAN_REVIEW`
- UTC timestamp: `2026-10-06T20:02:14Z`
- Display timezone: `America/New_York`
- Branch: `afad20a-rebuild`
- Scope: prove the supplied Dust 2 GLB is present and usable by the real
  server, and make that proof searchable in `events.jsonl`.

## What changed

The existing map loaders already used `assets/maps/dust2cyberiav4.glb`. This
session added structured ownership diagnostics without changing map geometry,
erosion, destructible-world behavior, or navigation authority.

- `src/world/world-gltf-loader.cpp`
  - Added `map.load-start`, `map.load-success`, and render-geometry
    `map.load-failed` events.
  - Records requested and resolved paths, source existence and bytes, render
    and collision counts, collision chunks, spawn count, revision, and bounds.
- `src/network/server-world.cpp`
  - Added `map.server-load-start`, `map.server-load-success`, and parse or
    empty-collision failure events for the headless authoritative server world.
  - Records the same path and source provenance plus collision triangles,
    spawns, and bounds.
- `config/debuglogger.json`
  - Changed the `world` category from `off` to `important` so these bounded
    lifecycle records reach the canonical `events.jsonl` stream.

Existing `printf` map-load output remains for the current terminal workflow;
the new structured records are the machine-readable evidence source.

## Build evidence

- Executable: `mimita-20261006T-dust2-map-load-v1.exe`
- Build command: `MIMITA_EXE_NAME=mimita-20261006T-dust2-map-load-v1.exe python build_agent.py`
- Result: successful; two changed translation units compiled and the new
  executable linked.
- Version check: `mimita-20261006T-dust2-map-load-v1.exe --versioninfo`
- Version-info journal: `logs/10-06-2026/20261006_160128/events.jsonl`

## Runtime evidence

The exact real-server command was:

```text
MIMITA_NPC_NAV_COMPARE=1 mimita-20261006T-dust2-map-load-v1.exe --server --bind 127.0.0.1:0 --map dust2cyberiav4 --gamemode counterstrike --npcs 2 --timeout 5 --no-map-rotation --no-discord-notification
```

The authoritative runtime journal was:
`logs/10-06-2026/20261006_160133/events.jsonl`.

Observed event chain:

1. `map.server-load-start` reported requested path
   `assets/maps/dust2cyberiav4.glb`, resolved path
   `C:\mimita-v9\assets\maps\dust2cyberiav4.glb`, `source_exists=true`, and
   `source_bytes=8628028`.
2. `map.server-load-success` reported `collision_triangles=7442` and
   `spawn_points=2`, with finite map bounds.
3. `npc.navmesh.bake-success` reported Recast/Detour availability and
   `navmesh_version=1` from the same server run, using 7,442 triangles.
4. `npc.nav.compare` records showed the custom navigator remained
   authoritative and Recast remained observational.

The journal contained 220 valid JSONL records. The map was therefore proven
to be present, resolved, parsed, converted to server collision geometry, and
used as Recast input in the real executable.

## Gate result and remaining limitation

This phase proves map availability and loading. It does not prove that the
Recast route is ready to become authoritative. The same run still produced
`npc.nav.compare` results with `failure=path_not_found` for the long Dust 2
NPC requests. The correct next investigation is Recast geometry/profile/
connectivity quality, not map upload or path discovery.

The custom navigation backend remains authoritative. Erosion tuning,
destructible-world invalidation, dynamic tile updates, traversal promotion,
Detour Crowd, and NPC behavior promotion remain later phases.

## Focused checks

- `docs/skills/logging-checker-v1.md`: pass. Events are owned by the world
  loaders, bounded to lifecycle transitions, include reasons and source paths,
  and do not claim route success.
- `docs/skills/asset-checker-v1.md`: pass for path and runtime-load checks.
  The GLB exists at the requested repository path and loaded on the server.
  Git tracking/commit status remains a repository-review concern because this
  session did not add or relocate the asset.
- JSONL parse check: pass for all 220 records in the runtime journal.

## Pre-existing work preserved

The worktree already contained unrelated documentation changes, including the
NPC navigation plan and prior ragdoll reference material. They were not
rewritten or attributed to this phase.

## Human review still needed

Human review should confirm that the intended Dust 2 map is visually the one
the player expects. The machine evidence proves the exact file path and server
geometry load, but it cannot by itself judge visual fidelity or Counter-Strike
route quality.
