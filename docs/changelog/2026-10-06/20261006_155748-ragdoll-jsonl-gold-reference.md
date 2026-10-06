# Task

- Task ID: ragdoll-jsonl-gold-reference
- Summary: Record the successful evidence-first ragdoll debugging method and
  ideal behavior specification as a reusable gold reference.
- Status: Complete
- Date, time, timezone: 2026-10-06T15:57:48-04:00
- Branch: current working tree
- Base commit: not recorded in this working tree
- Final commit: not committed by this session

# Requested behavior

Preserve how this session used live executable identity, canonical
`events.jsonl` logging, source tracing, and a full behavior specification to
diagnose and fix the intermittent ragdoll problem.

# Specification alignment

- Current specification paths:
  - `docs/specs/debug-logging/debug-logging.md`
  - `docs/specs/debug-logging/canonical-jsonl.md`
  - `docs/workflows/runtime-scenario-validation.md`
  - `docs/regressions/2026-10-06/ragdoll-per-death-presentation-ATTEMPT-1-REG.md`
- Exact requirement: use one structured logger, the active `events.jsonl`, a
  newly identified executable, bounded owner-level diagnostics, and separate
  source/build/runtime/human evidence.

# Exact implementation changes

## File: `docs/gold/2026-10-06-ragdoll-events-jsonl-live-debugging-reference.md`

- Added the session history, evidence chain, event ownership, fix summary,
  complete ideal-behavior specification, success conditions, and reusable AI
  workflow.
- Recorded the user's repeated successful Counter-Strike gameplay as human
  acceptance while keeping technical claims tied to journal evidence.

# Validation

- Documentation review: compared against the canonical JSONL and runtime
  validation specifications and existing gold-reference format.
- `git diff --check`: run after the documentation change.
- Runtime: no new executable was required for this documentation-only pass.
- Tests: none; no code or runtime behavior was changed.

# Regression review

- Regression entry appended: no.
- Why: this pass records the already documented ragdoll regression and its
  evidence-first fix as a permanent reference; it does not create a new
  failure.
- Related regression:
  `docs/regressions/2026-10-06/ragdoll-per-death-presentation-ATTEMPT-1-REG.md`

# Human acceptance

- The user reported repeated successful Counter-Strike deaths after the fix.
- The gold reference preserves that as human acceptance for the tested
  scenario, not as proof for every future mode or renderer path.
