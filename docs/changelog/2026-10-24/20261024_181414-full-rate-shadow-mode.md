# Full-rate shadow update mode

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-24 18:14:14 EDT (2026-10-24T22:14:14Z)
- User's requested current-mode note: mode 1 as of 2026-10-24 18:11 EDT (2026-10-24T22:11:00Z), because of fullscreen drawing issues.
- Branch: `20261011zt`
- HEAD: `0472121baef8d96342508d82aef5598de97daff7`
- Scope: restore shadow-map rendering every frame by selecting mode 1; retain mode 2's every-other-frame update cadence.

## Changes

`src/engine/engine-tick-render.cpp`, `engineTickRender()`:

- Before: `shadowFrameCounter` advanced every frame and `renderShadowThisFrame` was true only on alternating frames.
- After: local `constexpr int kShadowUpdateMode = 1` selects the original every-frame behavior. Mode 2 remains expressed as the alternate cadence. A one-time `render.shadow_update_mode` StructuredLogger event records the selected mode and whether it updates every frame.
- Reason: the player reported that fullscreen drawing looked choppy and depth/shadow presentation was wrong, after previously looking fine. Full-rate shadow updates are selected while those fullscreen drawing issues are investigated.

`docs/specs/open-gameplay-issues.md:191`: added the user report, the exact current mode and timestamp, the reason for mode 1, and the still-pending visual confirmation.

## Documents and review

- Read `docs/ROUTER.md`, `docs/specs/performance/performance.md`, `docs/specs/graphics/graphics-resolution.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, and `docs/architecture/time-and-formatting/time-and-formatting.md`.
- Focused skills: `docs/skills/efficiency-checker-v1.md` and `docs/skills/logging-checker-v1.md` — PASS. The selected cadence is logged once per process; no per-frame log was added.
- Behavior review: PASS against the requested mode definitions. Mode 1 draws the shadow map every rendered frame; mode 2 draws on alternating frames.
- Pre-existing `docs/specs/open-gameplay-issues.md` edits were preserved; the new report was added as one row in its existing issue table. Other unrelated working-tree changes were left untouched.

## Validation and remaining evidence

- `git diff --check`: PASS; Git printed line-ending normalization notices for unrelated pre-existing files.
- Canonical build: `python devscripts/dev-loop.py --request build --wait-for-build --build-timeout 240` — SUCCESS; published build 3102 at `.dev/builds/3102/mimita.exe`; compiled 1 source and skipped 567 unchanged sources.
- Dev-loop status confirmed build 3102 is current and source generation 4 was published. The existing visible client remains on build 3099; it was not stopped or relaunched.
- No tests were added or run. Runtime visual acceptance of mode 6 and its depth/shadow presentation remains pending; the build does not establish that the drawing issue is resolved.
