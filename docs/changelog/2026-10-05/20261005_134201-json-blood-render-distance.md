# Blood impact decal render distance

Date: 2026-10-05 13:42:01 America/New_York
Branch: current working branch

## Result

Added JSON-controlled render distance and distance-fade settings for impact
decal groups. Blood spray particles and blood surface splatter now read the
active blood settings from `config/impact_decals.json` instead of using the
previous hard-coded 40/60 meter blood limits.

## Exact changes

- `config/impact_decals.json:17-19`
  - Added `renderDistance: 1000.0`.
  - Added `renderFadeStartDistance: 850.0`.
  - Added `renderFadeEndDistance: 1000.0`.
- `src/config/impact-decals-config.h:54-56`
  - Added the three fields to `ImpactDecalGroupConfig`, retaining 60/40/60
    compatibility defaults for groups that omit the new keys.
- `src/config/impact-decals-config.cpp:108-110`
  - Added JSON parsing for all three fields.
- `src/effects/effect-part-render.cpp:531-573`
  - Blood particles use the configured render distance and fade interval.
  - Surface decals select the settings for their own group and use the
    configured render distance and fade interval.
  - Fade end is bounded by render distance, and the renderer retains only a
    small numeric safety clamp for zero-width fade intervals.

The unrelated generic effect pool still has its own 40-meter culling policy;
this change intentionally targets blood spray and blood surface decals, not
generic impact spheres or other effect types.

## Documents and review

- Read `AGENTS.md` and `docs/ROUTER.md`.
- Read `docs/specs/weapons/weapons.md`, `docs/specs/networking/networking.md`,
  `docs/operations/asset-management/asset-management.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/operations/build-and-exe/build-and-exe.md`, and
  `docs/operations/task-completion/task-completion.md`.
- Focused result: PASS for source/config ownership and behavior alignment.

## Validation

- JSON parsed successfully and reported the requested values: 1000, 850, and
  1000 meters.
- `git diff --check` passed.
- Canonical `python build_agent.py` completed with return code 0 and status
  `SUCCESS`; the affected objects were rebuilt before the final build output.
- No runtime or in-game visual acceptance was performed. Human review should
  confirm that a long-range Counter-Strike hit visibly produces blood at the
  intended viewing distance and that the higher effect visibility is an
  acceptable performance tradeoff.

## Pre-existing changes

The pre-existing user edit to `config/accounts/default.json` was preserved.
The build tooling touched the local analytics launch counter; its content was
restored to the pre-build value and has no functional relation to this change.
