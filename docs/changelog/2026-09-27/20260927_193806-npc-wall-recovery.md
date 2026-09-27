# Task

- Task ID: NPC wall recovery
- Summary: Make NPC local wall avoidance try side, diagonal, and backward directions, then backtrack and replan when the current route is blocked.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-09-27T19:38:06Z, display America/New_York 2026-09-27 15:38:06 EDT
- Branch: afad20a-rebuild
- Base commit: 00b01b17d1f196967e90274f31ef987645e9ab65
- Final commit: not committed

# Pre-existing changes

- Exact status output before edits: clean for the focused NPC implementation/config paths.
- Files not created or modified by this session: unrelated GUI, account, analytics, collision-mesh, collision-stress, and other changelog files were observed later and preserved.

# Requested behavior

When an NPC is moving into a wall, it should look for a nearby valid direction, including backing up, instead of continuing into the wall.

# Specification alignment

- Current specification paths: `docs/architecture/player-npc-systems/player-npc-systems.md`, `docs/architecture/collision/collision.md`, `docs/architecture/json-configuration/json-configuration.md`.
- Exact requirements: NPCs and players use shared gameplay/collision paths; collision runs at fixed 60 Hz; configuration has one owner and safe reload behavior.
- Why the change follows the specification: only the NPC decision direction changed; actual movement remains in the shared `physicsMainUpdate` path, and settings use `NpcDifficultyConfig`.
- Conflicts or decisions: wall candidate sensing remains the existing navigation ray helper; full capsule-aware candidate validation remains human-review follow-up.

# Exact implementation changes

## File: `src/npc/npc-navigation.cpp`

- Lines/functions/headings: `NpcNavigation::wallAvoidDirection`.
- Old behavior: after a blocked forward ray, test rotated side directions and otherwise return a perpendicular direction.
- New behavior: use `wallAvoidanceEnabled`, `wallCastDistance`, and `wallSearchDistance`; try left, right, diagonal, backward, and backward-diagonal directions. Return the first clear direction, or the direction with the greatest measured clearance if all are partly blocked.
- Reason: give the NPC a simple local escape behavior instead of keeping its original forward direction.
- Why unrelated behavior is preserved: navigator, traversal, jump, dash, and shared movement execution were not replaced.

## File: `src/npc/npc-navigator.h`, `src/npc/npc-navigator.cpp`

- Lines/functions/headings: `NpcNavigator::startBacktrack`, `NpcNavigator::update`, and `NpcNavigator::reset`.
- Old behavior: a blocked route could keep producing a direction toward the blocked goal until ordinary stuck handling reacted.
- New behavior: the navigator can enter a bounded backtrack state, move opposite the blocked direction for the configured distance/time, clear its cached route, and force a fresh local plan.
- Reason: a doorway can be hidden by the nearby wall; backing up exposes the opening so the local planner can see and use it.
- Why unrelated behavior is preserved: backtracking only activates after the requested route direction is blocked; normal open-space navigation is unchanged.

## File: `src/npc/npc-difficulty-config.h`, `src/npc/npc-difficulty-config.cpp`, `config/npc-difficulty.json`

- Lines/functions/headings: `NpcDifficultySettings`, JSON load/save, and active difficulty settings.
- Old behavior: no JSON controls for local wall recovery.
- New behavior: add `wallAvoidanceEnabled`, `wallCastDistance`, and `wallSearchDistance`, with validation and save support.
- Reason: keep the requested NPC behavior tuning in the existing hot-reloaded source of truth.

## File: `src/npc/npc.cpp`

- Lines/functions/headings: `NpcSystem::updateOneNpc` local navigation candidate gather.
- Old behavior: always gather a fixed 3m local collision region.
- New behavior: gather at least 3m or the configured wall search distance plus 0.5m.
- Reason: configured searches must have enough nearby triangles available.

# Diagnostics

- Owner/category: NPC navigation / NPC movement.
- Input: desired NPC planar movement direction and hot difficulty settings.
- Decision: retain direction when forward is clear; otherwise choose a nearby escape direction.
- Output: direction passed to existing NPC input and shared movement/collision code.
- Failure or rejection reason: no new failure path; if all directions are blocked, the greatest-clearance direction is used and existing stuck/repath logic continues.
- Rate limiting: existing navigation and stuck logging remain unchanged.

# Validation

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW; `docs/skills/efficiency-checker-v1.md` — reviewed cached local candidate reuse and fixed-tick execution.
- Tests and exact commands: `python devscripts/run-npc-combat-tests.py` — PASS; `git diff --check` — PASS.
- Build status: an earlier build passed before the backtrack change; the later full build was blocked by the unrelated pre-existing `src/gui/menus/pause-menu.cpp:197` error (`Terminal` has not been declared). No NPC-file compile error was reported.
- Runtime or hot-reload evidence: source/config/build evidence only; no live gameplay or hot-reload observation performed.
- Output files: `build/mimita.exe` and staged runtime DLLs produced by the canonical build.

# Measured evidence

- Before values: wall check was hardcoded to 1.5m; local navigation gather was fixed at 3m.
- After values: defaults are `wallCastDistance=1.5` and `wallSearchDistance=3.0`; values are clamped during load.
- Timestamps: build completed 2026-09-27T19:38:06Z or earlier.
- Tick/frame/network measurements: not collected.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: this is an intentional behavior change; no confirmed regression was observed.
- Related regression paths: none.

# Human acceptance

- Visual review: not performed.
- Gameplay review: still required in a map with NPCs facing walls and corners.
- Multiplayer review: not performed.
- Still unverified: live doorway/wall gameplay; whether ray-based local choices match the full NPC capsule clearance in every wall/corner case; whether the new behavior feels good.

# Related feature record

- Feature path: none.
