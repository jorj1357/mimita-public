# Configurable hit flinch

- EST timestamp: 2026-10-02 22:18:00
- UTC timestamp: 2026-10-03T02:18:00Z
- Branch: `afad20a-rebuild`
- Result: `BLOCKED`
- Focused skill: `docs/skills/spec-behavior-review-v1.md`

## Change

Added a `hitFlinch` object to `config/camconfig.json` and made it part of the
existing camera configuration owner. The default is enabled, deterministic,
and uses no random variation:

- `enabled`: master true/false switch.
- `low` and `high`: camera-punch degrees at low and high damage.
- `damageAtHigh`: damage value that reaches `high` strength.
- `pitch` and `yaw`: signed direction controls.
- `randomness`: deterministic direction variation; default `0.0`.
- `distance`: source-distance range at which the effect fades out.
- `distanceExponent`: shape of the distance falloff.

The existing `Camera::addPunch()` and decay behavior remain the effect owner.
Damage is applied only to the local player, and known damage sources now pass
their source position so distance falloff is meaningful for hitscan, melee,
rockets, and persistent explosions.

## Validation

- `git diff --check`: passed.
- The camera config contains comments, so Python's strict JSON validator is not
  applicable; the game's existing JSON parser is comment-tolerant.
- Full timestamped build was attempted with
  `MIMITA_FORCE_LINK=1 MIMITA_EXE_NAME=mimita-20261002T214500-hit-flinch.exe`
  and failed at the unrelated existing error:
  `src/npc/npc.cpp:658:21: invalid initialization of reference of type
  const World& from expression of type Player`.
- No executable was linked for this change, and no runtime/human acceptance was
  performed.

## Preserved work

Existing user changes and the earlier lens-distortion work were preserved.
The running fixed-name `mimita.exe` was not stopped or replaced.
