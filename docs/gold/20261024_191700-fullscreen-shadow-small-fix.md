# Gold example: a focused fullscreen rendering correction

- Date recorded: 2026-10-24
- User's current-mode note: 2026-10-24 18:11 EDT (2026-10-24T22:11:00Z)
- Build: 3102, published 2026-10-24T22:13:26Z
- Audience: MiMITA developers and AI agents handling user-reported rendering regressions
- Purpose: show how a short symptom report, a focused history check, and one small owner-level edit can produce a reviewable correction quickly.
- This document does not claim the fullscreen issue is visually resolved. The built correction was not launched into the active game, so human visual acceptance remains pending.

## The user's report and decision

The user first reported that fullscreen command 6 made the world look choppy and its depth presentation seem wrong. They then clarified that mode 6 had looked fine before, so the investigation treated the symptom as a possible regression rather than an inherent consequence of stretching.

The source history showed that commit `c1fe8acd` on 2026-10-22 changed shadow-map rendering from every rendered frame to every other frame. The shadow map contains scene depth used to render shadows; reusing it while the camera or actors move can make those depth cues lag behind the current frame. This was a plausible match for the user's description, not runtime proof that it caused every reported artifact.

The user then defined the desired choices:

- Mode 1: original behavior, render shadows every frame.
- Mode 2: render shadows every other frame.

They selected mode 1 for the current build because of fullscreen drawing issues and asked for a build.

## The small edit

The rendering owner is `engineTickRender()` in `src/engine/engine-tick-render.cpp`. The prior code advanced a frame counter and rendered shadows only when it was even. The correction added one local mode selection and kept both requested behaviors in that owner:

```cpp
constexpr int kShadowUpdateMode = 1;
const bool renderShadowThisFrame = kShadowUpdateMode == 1 ||
    (kShadowUpdateMode == 2 && (shadowFrameCounter++ & 1u) == 0u);
```

Mode 1 is the current selection. Mode 2 remains available by changing the mode constant. A one-time `render.shadow_update_mode` StructuredLogger event records which cadence the executable selected. The change did not add a second shadow system, alter the fullscreen stretch path, or introduce an unrequested settings surface.

The current-mode note was also recorded in `docs/specs/open-gameplay-issues.md`, including the user's timestamp and the reason for selecting mode 1.

## Build evidence and its limit

The canonical dev-loop build published `.dev/builds/3102/mimita.exe` successfully. It compiled one changed source file and skipped 567 unchanged sources. The active client remained on build 3099 and was not stopped or relaunched.

This confirms the source compiles and the new executable was published. It does not confirm mode 6 looks right or that the reported choppiness and depth issue is gone. Those need a visual comparison in the real game using the new build.

## Why this was a good small fix

The useful sequence was:

1. Start from the user's visible symptom.
2. Use repository history to identify a recent change at a plausible rendering owner.
3. Let the user define the two cadence modes and choose the current one.
4. Change the single owner that selects shadow update cadence, while retaining the alternative.
5. Publish a normal build, leave the active client intact, and state exactly what still needs visual review.

The user described the interaction as “1 plan prompt then 1 little edit then it fixed it.” The small scope is worth preserving: it made the proposed cause, selected behavior, and build result easy to review. In this record, “fixed” means the requested mode-1 correction was built; the fullscreen visual result is not yet accepted.

## Evidence references

- Source and build record: [`../changelog/2026-10-24/20261024_181414-full-rate-shadow-mode.md`](../changelog/2026-10-24/20261024_181414-full-rate-shadow-mode.md)
- Current issue and mode note: [`../specs/open-gameplay-issues.md`](../specs/open-gameplay-issues.md), “Fullscreen drawing and shadow depth”
- Fullscreen behavior contract: [`../specs/graphics/graphics-resolution.md`](../specs/graphics/graphics-resolution.md)
- Performance rules: [`../specs/performance/performance.md`](../specs/performance/performance.md)
