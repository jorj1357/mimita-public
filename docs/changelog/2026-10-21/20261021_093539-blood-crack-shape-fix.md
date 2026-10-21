# Blood and world-crack PNG shape fix

- **Timestamp:** 2026-10-21 09:35:39 EST
- **Branch / HEAD:** `20261011zt` / `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- **Result:** `PASS_WITH_HUMAN_REVIEW`

## Cause and change

The visual spec at `docs/specs/visuals/blood-bulletholes-viscera.md` §8 requires complete authored PNGs without distortion. The implementation violated it in two places: textured blood spray particles randomized their billboard aspect ratio, and world cracks repeated the full crack PNG over random narrow segment quads. I changed blood spray to equal billboard axes and cracks to one square, surface-aligned stamp per impact. The crack stamp can rotate in its surface plane; its configured full diameter is 2 world units. Removed the now-unused crack-arm, crack-chain, and thickness settings/parser. Existing blood and bullet-hole decals already use equal-sided quads. Added min/max aspect and visible-count fields to the existing once-per-second `presentation.render_visibility` diagnostic.

## Files and exact edits

- `src/effects/effect-part-blood.cpp:345-348`, `EffectPartSystem::spawnBloodEffect()`:
  - Before: `particle.stretch = 0.7f + (float)(rand() % 601) / 1000.0f;`
  - After: comment states both billboard axes must stay equal; `particle.stretch = 1.0f;`.
- `src/effects/effect-part.cpp:359-391`, `EffectPartSystem::spawnWorldCracks()`:
  - Before: `const int armCount = std::clamp(c.crackArms.baseCount, 1, c.crackArms.maxCount);` followed by nested randomized arm/segment loops.
  - After: comment says “Use the complete square crack PNG as one stamp”; projects shot direction onto the surface, rotates one axis in-plane, and pushes one decal whose radius is `max(0.01, length * 0.5) * sizeScale`.
- `src/effects/effect-part-render.cpp:663-675`, surface decal batch:
  - Before: crack quad width came from `decal.height`, while its other axis came from `decal.radius * texScale`, mapping the complete image onto a narrow segment.
  - After: both axes use `half = max(0.001, decal.radius * texScale)`; current square textures map the full UV range to an equal-sided quad. Counts/aspect min/max are reported at lines 585-588 and 668-675; summary fields are at lines 721-725.
- `config/impact_decals.json:136-140,165-170`:
  - Before: `"length": 10.0`, active `"thickness": 0.015`, plus nested `"arms"`/`"chain"` and center/outer thickness values.
  - After: comment “Full square PNG stamp diameter in world units.”, `"length": 2.0`, and only `"randomRotationDegrees": 360.0` remains for crack variation. Removed obsolete active chain/arm/thickness options. The working tree also contains pre-existing blood radius/alpha edits (`0.85`/`0.7` vs HEAD `1.85`/`0.9`); those are not part of this fix and were preserved.
- `src/config/impact-decals-config.h:120-124`:
  - Before: `CrackArmsConfig`, `CrackChainConfig`, `crackCenterThickness`, and `crackOuterThickness` members.
  - After: those unused members are removed; spray/client feedback/force config remains.
- `src/config/impact-decals-config.cpp:181-185`:
  - Before: parsing for `arms`, `chain`, `center_thickness`, and `outer_thickness`.
  - After: obsolete parsing removed; existing spray and client feedback parsing remains.
- `docs/specs/open-gameplay-issues.md:74-75,82`:
  - Before: said the root draw sequence remained unconfirmed and listed corrective directions as future work.
  - After: records the traced sources, implemented/build-2741 fix, pending live visual verification on the active build-2733 session, and separate unidentified red/transparent geometry.
- `docs/changelog/2026-10-21/20261021_093539-blood-crack-shape-fix.md`:
  - New file; no prior text. This entry records the implementation and evidence.

The repository already had many unrelated modified and deleted files, and prior changelogs from other tasks. They were preserved. No confirmed regression record was added because this session fixed a reported issue without reproducing it in a fresh runtime.

## Documents and focused review

- Read `AGENTS.md`, `docs/ROUTER.md`, the blood/bullet-hole/viscera visual specification, code ownership, JSON configuration, time/formatting, runtime-scenario validation, build/EXE, live-development, and task-completion documents.
- Focused skills read: `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, `docs/skills/asset-checker-v1.md`, `docs/skills/efficiency-checker-v1.md`. The aspect fix aligns with the visual contract; logging is bounded by the existing one-second summary. The square texture assumption applies to the three active configured PNG assets (all 256x256 RGBA). The obsolete segmented chain was removed instead of adding a second rendering path.
- Router-selected `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md` were absent in this checkout.

## Validation and remaining proof

- Config structure parsed successfully after stripping the file's `//` comments; `worldCracks.length` is `2.0`. No test suite was added or run.
- The first dev-loop build request failed with Windows `WinError 5` while replacing `.dev/current-run.json.tmp`; retry succeeded: `[DEV CONTROL] build request=2026-10-21T09:33:08.056Z-b8e9a47d result=success build=2741`.
- Build 2741 proves compilation/linking only. Its exact executable `C:\mimita-v9\.dev\builds\2741\mimita.exe --versioninfo` reported `EVENTS_JSONL_PATH=logs/10-21-2026/20261021_053325/events.jsonl`; that versioninfo-only run contained no gameplay impact/render events.
- The active Duel session remains on build 2733 (`run 20261021_084608`, map `xxity2`). It was left running. No fresh gameplay scenario was exercised, so the PNG appearance and new aspect diagnostic have not been observed live. Human visual acceptance is pending. The screenshots' large red/transparent geometry also remains unidentified and may not be caused by these textured stamps.
- `git diff --check` reported pre-existing trailing whitespace in unrelated `docs/specs/gamemodes/juggernaut.md` lines 7, 9, and 12; no cleanup was made.
