# NPC continue-pursuit behavior

Date: 2026-10-04 15:55 EST
Branch: `afad20a-rebuild`
Result: PASS_WITH_HUMAN_REVIEW

## Request

When an NPC loses sight of a player, keep it committed to the visual
last-known position. After reaching that position, search locally for 600
fixed ticks, then return to forward patrol.

## Changes

- Added `after_reaching_last_known: "continue_pursuit"` behavior.
- Added JSON-controlled `pursuit_search_ticks`, defaulting to 600 ticks.
- Updated Sandbox's `rage2` profile to use `continue_pursuit`.
- Kept the NPC in Chase while traveling to the remembered position instead of
  falling back to Patrol when Counter-Strike disallows Circle.
- Added a local search direction after arrival, based on the target's last
  known movement or the NPC's current facing, for the configured duration.
- Made hidden-target navigation use `targetMemory.lastKnownPosition` so later
  hearing reports do not redirect a continuing visual pursuit.
- Memory expiry still ends pursuit and returns the NPC to Patrol.

## Validation

- `python build_agent.py`: BUILD SUCCESS; executable linked.
- `mimita.exe --npc-perception-selftest`: PASS.
- `mimita.exe --npc-search-behavior-selftest`: PASS.
- `mimita.exe --npc-movement-policy-selftest`: PASS.
- `mimita.exe --gamemode-selftest`: PASS.
- `git diff --check`: no new whitespace errors in the task files.

## Human review still required

Run Sandbox with the `rage2` profile, hide behind cover, and confirm that the
NPC reaches the last visible position, searches locally for about 10 seconds,
and then resumes forward patrol. Confirm that local wall avoidance still lets
it route around real obstacles without reversing away from the remembered
position.

## Pre-existing work preserved

Existing unrelated worktree edits were preserved.
