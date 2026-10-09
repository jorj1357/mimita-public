# Juggernaut3 map-load investigation

- Time: 2026-10-09T03:21:33Z (display: 2026-10-08 23:21:33 EDT)
- Result: PASS_WITH_HUMAN_REVIEW
- Scope: Investigation only. No source, JSON, asset, or runtime changes were made.
- Branch/state: detached HEAD at `3ab837ee`; pre-existing working-tree edits were preserved.
- Existing user files include modified configuration/source files, untracked `assets/maps/juggernaut2.glb`, `assets/maps/juggernaut3.glb`, and an existing untracked `docs/changelog/2026-10-09/` directory.

## Documents and focused reviews

- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/operations/asset-management/asset-management.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/regressions/regressions-v1.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/asset-checker-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`

## Evidence

- User-provided build text reports `juggernaut3`, `assets/maps/juggernaut3.glb`, file size `3820488`, and server collision loading with `19982` triangles and `2` spawn points.
- `logs/2026-10-09/20261009_031853/events.jsonl` records server `map.server-load-start` and `map.server-load-success` for `assets/maps/juggernaut3.glb` (lines 15656 and 15663 in the current journal), followed by client `map.load-start` and `map.load-success` (lines 15801 and 15807).
- The same journal records `npc.navmesh.bake-failed` with `failure=detour_data_failed`, `walkable_triangles=0`, and `build_ms=5101.2252` (line 15759). This is the first confirmed failure after the GLB load.
- `config/gamemodes/juggernaut.json:75` lists `dust2cyberiav4`, `atdm`, `mimita-duels-map-v3`, and `funworld`; it does not list `juggernaut3`.
- `src/network/server.cpp:314-335` uses the JSON mode map pool for mode defaults/rotation, while the explicit map path remains accepted at `src/network/server.cpp:444-450`.
- `src/engine/engine-tick-net.cpp:457-523` loads a server-required map and sends `ClientMapReady` after the client world is ready; this path succeeded for `juggernaut3` in the journal.
- `src/network/server-npcs.cpp:160-231` owns the NPC collision/navmesh preparation and emits the observed bake failure.
- `src/gamemode/map-config.h:68-70` documents that a missing per-map JSON file falls back to defaults with no sites; this is not the GLB loader failure.

## Finding

- Severity: high for NPC-enabled Juggernaut runtime; medium for map selection
- Type: spec-code disagreement / missing runtime acceptance
- Specification: Juggernaut's active JSON map list is the mode-owned map source, but the requested asset is not present in that list.
- Code path: `config/gamemodes/juggernaut.json:75`, `src/network/server.cpp:314-335`, `src/network/server-npcs.cpp:160-231`.
- Actual behavior: explicit launch selects and loads `juggernaut3.glb`; NPC Recast preparation then fails with no walkable triangles. The map is therefore not rejected by the GLB loader, but the runtime can appear stuck or unusable when the NPC/navigation path is expected to be ready.
- Expected behavior: if `juggernaut3` is intended for Juggernaut, it must be included in the mode's authoritative map list and must produce a valid navigation surface for the configured NPC behavior, or the mode must explicitly support a bounded fallback when navigation is unavailable.
- Recommended action: first decide whether `juggernaut3` is an official Juggernaut map. If yes, add it through the approved mode-map configuration change and separately diagnose why its authored triangles produce zero Recast walkable triangles. Do not treat adding the name alone as a runtime fix.
- Human review still required: confirm the intended map pool and observe a fresh real Juggernaut run after the navmesh issue is addressed.

## Validation boundary

- Source/config/log inspection: PASS.
- Build validation: NOT_APPLICABLE; no code changed.
- Runtime map-load evidence: PASS; both server and client loaded the GLB.
- Runtime Juggernaut/NPC navigation acceptance: NOT_PROVEN; the live journal contains the Recast bake failure.
- Visual/human acceptance: NOT_PERFORMED.
