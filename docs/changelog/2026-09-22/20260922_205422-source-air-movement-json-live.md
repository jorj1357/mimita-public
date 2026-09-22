# Source Air Movement JSON Live Path

Date: 2026-09-22
Status: hot-code fix built; human live acceptance pending

## Work performed

- Kept `config/movement/movement-source.json` unchanged as the requested
  movement source.
- Kept `movement-cs.json` out of the active path.
- Preserved the shared JSON path resolver so `source` resolves directly to
  `config/movement/movement-source.json`.
- Changed the hot v206 air policy so airborne WASD cannot launch an actor from
  rest and speed gain comes from projection against existing horizontal motion,
  matching the requested CS/GoldSrc-style air-strafe feel.
- Added the missing final horizontal speed clamp to hot `movement.main` after
  dash, down-dash, jump, and collision response, so JSON
  `speed_limit_enabled: true` and `speed_limit: 50.0` apply to the live path.
- Changed the hot landing/bhop branch to skip ground acceleration while jump is
  held, preserving emergent Quake-style behavior without remembering the held
  movement direction.
- Added a regression record documenting the behavior break and the human
  understanding/rapid-work concern.

## Evidence

- Source evidence: `config/movement.json` selects JSON preset `source`.
- Source evidence: `movement.main` passes the resolved tuning to the hot air
  policy.
- Source evidence: `movement-air.cpp` now contains the no-launch projection
  branch.
- Build evidence: hot candidate generation 5 built successfully as
  `build/hotreload/p24400/gen5/mimita-live-g000005.dll`; the follow-up speed
  clamp candidate generation 6 built successfully as
  `build/hotreload/p24400/gen6/mimita-live-g000006.dll`; `mimita.exe` was not
  written by the live-build path.
- Follow-up build evidence: generation 7 built successfully as
  `build/hotreload/p21184/gen7/mimita-live-g000007.dll`.
- Runtime evidence: activation in a running game and in-game review are still
  pending.
- Human acceptance: pending.

## Limits

This change does not claim that the movement now feels correct until the human
tests it in the running game. The required test is standstill jump plus WASD,
then existing horizontal movement plus mouse turning and A/D strafing.
