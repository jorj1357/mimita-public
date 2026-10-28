# Record animation and death choppiness findings

- Recorded: 2026-10-28 09:37 EDT (2026-10-28T13:37:00Z)
- Branch: `20261011zt`
- Starting HEAD: `1cad86d208b4b9915ebc06cb77dc36982350b60a`
- Completion commit: none; documentation is uncommitted for review
- Result: `PASS_WITH_HUMAN_REVIEW`

## Existing work preserved

The working tree was clean before these documentation changes. The implementation and performance evidence were already recorded in `docs/changelog/2026-10-28/20261028_041903-plan-a-gameplay-diagnostics.md`; no gameplay code, configuration, or issue tracker was changed in this session.

## Changes

- Added `docs/gold/20261028_093400-animation-and-death-choppiness.md` with the identified animation sampling/allocation, hot-reload polling, walk/run flapping, corpse solver/diagnostic, and synchronous Opus decode causes; the measured improvements; the player's 09:34 EDT report that choppiness seems much better; and the reusable per-actor/per-tick scaling lesson.
- Added `docs/regressions/2026-10-28/animation-death-frame-choppiness-REG.md` as a dedicated occurrence record linked to the implementation/runtime changelog. It records corrected owners and evidence, while leaving full performance acceptance open because the 240-FPS-for-60-seconds target was not met and other spikes remained.

## Instructions and review

Read `AGENTS.md` as supplied by the user, `docs/ROUTER.md`, `docs/specs/performance/performance.md`, `docs/skills/efficiency-checker-v1.md`, `docs/regressions/README.md`, `docs/operations/task-completion/task-completion.md`, and `docs/architecture/time-and-formatting/time-and-formatting.md`. The router references `docs/doc-review-09-03-2026.md`, but that file is absent in this checkout.

The regression is marked `ATTEMPTED FIX (1)` rather than a fully closed solution: build/runtime measurements show substantial improvements and the player reports the choppiness seems much better, but the overall performance target remains unmet. This preserves the user's improvement report without overstating acceptance.

## Validation and remaining review

- `git diff --check`: passed.
- Confirmed the linked changelog and runtime journal exist in this checkout.
- Documentation-only work; no build, runtime session, or new test was run.
- Human confirmation that the improvement persists in representative play and whether remaining performance is acceptable are still open.
