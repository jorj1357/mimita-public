# Fighter camera precedence fix

Date: 2026-10-08

## Change

- Fighters now force their configured FOV of 100 instead of inheriting the
  shared Juggernaut presentation FOV of 50.
- Juggernaut mode now resolves the local team actor preset before finalizing
  the first-person decision.
- Fighter clients therefore leave the shared Juggernaut first-person policy and
  restore third-person, while Juggernaut clients remain forced first-person.

## Evidence

- Source/config evidence: `config/actor-presets/juggernaut_fighter.json` now
  uses `force_fov: true`; `src/network/community-match-client.cpp` applies the
  resolved local team preset as the final Juggernaut camera authority.
- Build: `mimita-20261008T-fighter-camera-fix.exe` completed successfully;
  1 translation unit compiled and 548 were reused.
- Version info: `logs/10-08-2026/20261008_152703/events.jsonl`.
- Runtime/human visual acceptance: not performed in this turn; the game was
  not launched.
