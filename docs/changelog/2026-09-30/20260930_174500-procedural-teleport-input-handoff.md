// 2026-09-30T21:45:00Z (display: 2026-09-30 17:45:00 EDT)
/* purpose
* Stop procedural_world_teleport_highest from sending stale predicted movement
* while the server's authoritative teleport epoch is travelling to the client.
*/

# Task

- Summary: Fix procedural highest-room teleport rubberbanding caused by old
  client movement reports crossing the server teleport handoff.
- Status: CODE_COMPLETE / BUILD_BLOCKED_BY_PRE_EXISTING_UNRELATED_ERROR /
  RUNTIME_VALIDATION_REQUIRED

# Changes

- `src/network/multiplayer-context.h`: added client-side procedural teleport
  handoff state with the starting epoch and bounded timeout.
- `src/network/multiplayer-packets.cpp`: marks the handoff before sending
  `procedural_world_teleport_highest`.
- `src/network/multiplayer-tick.cpp`: suppresses only outgoing movement packets
  during the handoff, then resumes immediately after completion or timeout.
- `src/network/multiplayer-reconcile.cpp`: clears the handoff after applying a
  newer authoritative transform epoch and exact server position.
- `src/network/server-packets.cpp`: prevents in-flight movement reports from
  becoming visible `POSITION CORRECTION` disagreement events while the server
  transform acknowledgement gate is active.

# Validation

- `git diff --check`: changed teleport files have no whitespace errors.
- `python build_agent.py`: attempted. The build reached the changed network
  translation units but ended with a pre-existing unrelated declaration/definition
  mismatch in `src/impact/destructible-geometry.h` and
  `src/impact/destructible-geometry.cpp` (`const DestructionCut&` versus
  `DestructionCut` by value). That user-owned mismatch was not changed.
- Runtime multiplayer acceptance remains required: run the command once and
  verify the client logs `CLIENT PROCEDURAL TELEPORT BEGIN` followed by
  `CLIENT PROCEDURAL TELEPORT COMPLETE`, with no position-correction storm.
