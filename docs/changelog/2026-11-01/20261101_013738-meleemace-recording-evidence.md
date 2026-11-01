# Melee mace recording evidence and scenario readability issue

Time created: 2026-11-01T01:37:38Z
Time last updated: 2026-11-01T01:37:38Z

Status: EVIDENCE INTAKE ONLY; NO GAMEPLAY FIX IN THIS PASS

## Goal

Analyze `scenario-20261101_011715-meleemace.jsonl` for geometry catches, mace
ground clipping, and NPC world penetration, and record the required automatic
readable-scenario behavior in the open gameplay issues document.

## Evidence

- Recording is complete and merged: 7275 client ticks, 7250 server ticks,
  header and footer present, `client_input`, `client_actors`,
  `client_collision`, and `server_actors` present.
- Client grounded speed collapses at frame 4161 and frame 6429.
- After frame 6980 the player has no ground contact and both client and server
  fall far below the map.
- Client/server root divergence starts at frame 62 and exceeds 1.5 m for 5156
  frames; maximum distance is 16.37 m at frame 529.
- Server NPC capsule recovery moved NPC 100006 from z 2349.18 to z 2303.11 at
  server tick 18419; it later reached z -500. NPCs 100008, 100005, and 100017
  also fell far below the map.
- The same recording produced 999 `npc.stuck-recovery` and 106
  `npc.capsule.recovered` events, but zero `npc.pose.penetration` and zero
  `npc.pose.reset` events.
- Recovery code uses a 100 m downward `groundHeightAt` query and checks only
  the root point after moving the NPC to a lower floor.

## Documentation changed

- Added the automatic readable scenario format requirement to
  `docs/specs/open-gameplay-issues.md`.
- Added the melee mace/NPC evidence intake and next-fix priorities to the same
  document.

## Validation

- `scenario-analyze.py` completed on the full recording.
- `scenario_play` could not start: `start_readiness_timeout`; the server
  position did not match the recorded start and server begin was not prepared.
- No gameplay fix, build, live replay, or human visual acceptance is claimed.

## Remaining work

1. Replace downward-through-solid NPC recovery with last-clear/final-pose safe
   recovery and full-body validation.
2. Re-run NPC final-pose containment after actor-pair and recovery.
3. Add player final-pose containment after animation/AimBody and actor-pair.
4. Fail closed on final-pose narrowphase budget exhaustion.
5. Record final-pose state and client/server root deltas in scenario tracks and
   generate the readable summary automatically.
