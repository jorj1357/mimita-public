# NPC default behavior and decision diagnostics explanation

- **Time UTC:** `2026-10-25T17:59:18Z`
- **Display time:** `2026-10-25 13:59:18 EDT` (`America/New_York`)
- **Branch:** `20261011zt`
- **HEAD:** `0f33025c`
- **Result:** `PASS_WITH_HUMAN_REVIEW`

## Request

Further explain the role of C++ versus JSON in NPC behavior, the legacy/default
behavior path, TDM target behavior, circling, possible decision logging, and a
fair aggressive movement/combat direction. No code changes requested.

## Findings

- Role-less NPCs without a behavior profile enter the compatibility path:
  state scoring in `pickNextState` (`src/npc/npc-state-machine.cpp`), hard-coded
  state movement/orbit/strafe (`src/npc/npc-states.cpp`), legacy distance-based
  weapon switching (`src/npc/npc.cpp`), and custom local navigation by default.
- TDM's current config selects opposite-team targeting including humans and
  NPCs, but supplies no mode NPC behavior profile or team NPC role. The server
  chooses a nearest hostile; a live journal is needed to see whether a specific
  circling actor had a valid target and which state/goal/path it followed.
- `selectUtilityGoal` in `npc.cpp` computes and records a utility goal/action;
  actual movement state and inputs still flow through `pickNextState` and
  `computeStateMovement` in the legacy brain. Avoid assuming utility scores are
  already the controlling tactic selector.
- Existing structured events include target/state/goal changes, utility
  decisions, movement snapshots, nav plan/failure/repath, commitment and stuck
  recovery, weapon changes, shots, and reactions. Check the live journal before
  adding new events. A missing state-score/reason event could be added later at
  the selector owner, change-edge/sampled and rate-limited.
- Current down-dash request gate in `npc.cpp` requires a target, airborne state,
  height over target, cooldown completion, and random chance. Ground use would
  require revisiting that AI request gate while retaining shared movement
  ability validation.
- A useful future design keeps the C++ tactical selector, route recovery,
  momentum-aware movement choices, evasive actions, and legal weapon tactics as
  reusable logic; JSON then chooses/tunes policies. No implementation or live
  run was performed.

## Changes and validation

- Added this changelog only. No source or gameplay config changed.
- Read-only source/config inspection. Runtime cause of a particular circling
  NPC remains unconfirmed; no live journal was examined or human playtest run.
