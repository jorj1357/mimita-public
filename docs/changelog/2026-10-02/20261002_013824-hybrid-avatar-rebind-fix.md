# Rebind hybrid aim body after avatar changes

Time (UTC): `2026-10-02T01:38:24Z`
Branch: `afad20a-rebuild`

## Result

When a player changes avatars while the normal-play physical aim body is
active, the aim body now detects that its bind belongs to a different avatar
and rebuilds from the new avatar's rest pose before the next simulation tick.
This preserves the selected hybrid mode and avoids reusing the previous
avatar's `meshLocal`, node mapping, and physical bind state.

## Changes

- `src/ragdoll/ragdoll-mode.h`: added `aimBindingNeedsRebind()` and stored the
  avatar name used to build the active aim body.
- `src/ragdoll/ragdoll-mode.cpp`: records the bound avatar on aim-body build,
  clears it on deactivation, and reports a mismatch when the player avatar
  changes.
- `src/sim/simulate-tick.cpp`: rebinds on either the existing lifecycle change
  or an active aim-body/avatar mismatch.

## Validation

- `git diff --check`: passed.
- `python build_agent.py`: `BUILD SUCCESS`, 2 translation units compiled, 506
  skipped, return code 0.
- Human in-game validation is still required. Test Jason -> demongirl while
  hybrid is active, then switch to default and back to hybrid.

