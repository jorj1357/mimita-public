# NPC C++ and navigation source map

- **Time UTC:** `2026-10-25T17:49:28Z`
- **Display time:** `2026-10-25 13:49:28 EDT` (`America/New_York`)
- **Branch:** `20261011zt`
- **HEAD:** `0f33025c`
- **Result:** `PASS_WITH_HUMAN_REVIEW`

## Request

Explain the C++ and external-library NPC behavior path and how it could support
strong, fair TDM/Duel opponents. User explicitly said not to change code.

## Findings

- NPC orchestration and intent-to-input flow: `src/npc/npc.cpp`.
- Mode/team target assignment: `src/network/server-npcs.cpp`; TDM currently
  sets `npc_targeting.mode=opposite_team` and includes players and NPCs, but has
  no mode behavior profile or explicit team NPC roles. NPCs therefore use the
  legacy/default behavior path unless individually profiled.
- TDM target selection chooses nearest live hostile actor; profile scoring can
  add stickiness and other weights in modes not using `opposite_team`.
- Tactical states: `src/npc/npc-state-machine.cpp` and `src/npc/npc-states.cpp`.
  Utility scoring is in `npc-utility.cpp`; combat movement intent is still
  coordinated in `npc.cpp`.
- Navigation: `src/npc/npc-navigator.cpp`, `npc-navigation.cpp`,
  `npc-nav-graph.cpp`, and `npc-traversal.cpp`; stuck/open-space correction is
  in `npc-navigator.cpp` and `npc.cpp`.
- Recast/Detour adapter: `src/npc/recast-navigation.cpp` and `.h`. The adapter
  builds/query nav data; `npc-navigator.cpp` can promote a Recast route when a
  profile or development override selects it and retains the custom planner as
  fallback. `DetourCrowd` avoidance is not the current active crowd solution.
  `external/npc-navigation-sources.md` still says the sources are not wired to
  runtime/build, but `build.py` includes Recast/Detour sources and
  `recast-navigation.cpp` is in the runtime path; the inventory note is stale.
- Dash/down-dash are currently situational/random decisions in `npc.cpp` and
  obey shared movement availability. Freeze is not an authored offensive
  combo; `freezeChance` is a very rare panic movement choice. Weapon scoring and
  switching are in `npc.cpp`; actual legal firing is in `npc-combat.cpp`.
- The behavior spec says authored weapon rotation and conditional movement
  sequences are not implemented. A strong player-like opponent needs better
  decision design plus repeated runtime playtesting, not simply bigger numbers.

## Changes and validation

- Added this changelog only; no source/config/runtime behavior was changed.
- Read-only source and config inspection. No build, runtime scenario, or human
  acceptance was performed. Preserve all pre-existing working-tree edits.
