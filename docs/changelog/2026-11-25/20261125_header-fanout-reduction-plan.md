# Header fanout reduction plan

Recorded a staged plan in `issue-136d4553ff92` for measuring and reducing unnecessary C++ header-driven rebuilds.

## Evidence used

- The current issue entry from `root-header-fanout-plan` reports 40 observed invalidations attributed to `src/network/packets.h` in gen634 and 13 from `src/combat/weapon-fire.h` in gen635. It also reports an active compiler-graph census; use that census before selecting a pilot.
- Earlier dependency-cache evidence reported 267 translation units depending on `src/combat/weapon-types.h`, with 180 quoted include paths through `src/entities/player.h`; this is an older candidate and does not supersede measured invalidations or the active census.
- The latest root dev-loop check had no active build or pending batch. `src/entities/player.h`, `src/vehicles/vehicle-types.h`, and `src/vehicles/vehicle-system.cpp` have unrelated worktree changes and must remain with their current owners.

## Plan

1. Build a read-only per-header report from cached depfiles: direct and transitive include paths, exact dependent translation units, cache invalidation reasons, per-TU compile wall/CPU time, and header edit frequency. Distinguish the scan denominator from the actual compile set.
2. Rank candidates by measured avoidable dependent closure and compile cost, not raw include fan-in. Keep all dependency freshness checks and compile every true dependent.
3. Wait for the current graph census and source-owner release. Start with the highest measured, owner-approved header seam; the current issue ranks `packets.h`, with `weapon-fire.h` as a fallback. Do not start with the older `weapon-types.h`/`player.h` proposal.
4. Make one behavior-neutral boundary change per Mode 3 batch, such as a narrow stable type header, legal forward declarations, or moving non-template implementation bodies into the owning `.cpp`. Do not change serialized layouts or gameplay behavior in a fanout-only pilot.
5. Compare exact before/after depfile closures through the root dev-loop. Include a `.cpp`-only control and record changed-TU count, per-TU/aggregate compile time, preflight, link, publish, host/process CPU/RAM, and runtime DLL activation separately.
6. Evaluate PCH and unity builds only as separate controlled experiments. PCH may reduce parse work but does not remove true dependencies and can broaden invalidation; unity builds can make a changed unit larger.

## Limits and validation

Header cleanup can prevent unnecessary recompilation; it cannot skip rebuilding a true dependent after a semantic interface change. It also cannot remove the monolithic DLL link floor or make a genuinely expensive translation unit compile faster. No C++ source, build configuration, or runtime process was changed or tested for this planning update.
