# Recast navigation failure diagnostics

- Time: 2026-10-09T03:24:20Z (display: 2026-10-08 23:24:20 EDT)
- Result: PASS_WITH_HUMAN_REVIEW
- Scope: Added bounded structured diagnostics; did not change navigation behavior or map selection.
- Branch/state: detached HEAD at `3ab837ee`; all pre-existing modified and untracked files were preserved.

## Change

- `src/npc/recast-navigation.h:37-52` now carries progressive Recast build facts: converted grid dimensions, agent cell parameters, contour/poly/detail counts, and Detour data size.
- `src/npc/recast-navigation.cpp:182-321` records those facts as each build stage completes, including the 3,373 walkable triangles found before the current `detour_data_failed` boundary.
- `src/npc/recast-navigation.cpp:395-429` emits one `npc.navmesh.prepare-failed` event per failed preparation attempt through the canonical `NPC_MOVEMENT` JSONL channel. It includes the failure reason, stage counters, profile values, converted bounds, world revision/triangle count, and build duration. No per-frame or per-NPC spam was added.

## Validation

- Focused skills read: `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, `docs/skills/asset-checker-v1.md`.
- Canonical build: `python build_agent.py` with `MIMITA_EXE_NAME=mimita-20261009T032407.exe`; status `SUCCESS`, linked `C:\mimita-v9\mimita-20261009T032407.exe`.
- Real headless Juggernaut run: `mimita-20261009T032407.exe --server --map juggernaut3 --mode juggernaut --bind 127.0.0.1:1368 --npcs 3 --timeout 8`.
- Fresh journal: `logs/10-08-2026/20261008_232420/events.jsonl`.
- Fresh event proof: `npc.navmesh.prepare-failed` reports `detour_data_failed`, `source_triangle_count=19982`, `walkable_triangle_count=3373`, `recast_grid_width=4974`, `recast_grid_height=4974`, `contour_count=0`, `navmesh_poly_count=0`, and `build_ms=6104.7601`. The existing `npc.navmesh.bake-failed` then records the same failure through the server NPC owner.
- Map-load proof remains separate: the same journal records `map.server-load-success` for `juggernaut3.glb` before the navigation failure.

## Remaining review

- The logging fix is verified. The root behavior fix is not included: the new evidence now points to Recast producing walkable triangles but zero contours after filtering/region construction, before Detour data creation. A separate approved change should diagnose or adjust that navigation input/parameter path.
- Human gameplay/visual acceptance remains required.
