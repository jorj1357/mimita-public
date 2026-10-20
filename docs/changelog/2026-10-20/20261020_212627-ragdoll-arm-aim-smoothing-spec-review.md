# Task

- Task ID: ragdoll-arm-aim-smoothing-spec-review
- Summary: Assessed whether the current ragdoll specification defines smooth arm aiming well enough for implementation.
- Status: NEEDS_SPEC_DECISION
- Date, time, timezone: 2026-10-20T21:26:27Z (17:26:27 EDT, America/New_York)
- Branch: 20261011zt
- Base commit: 7556a218ec6b6bb8443485ff47b51c9ebf844e0d
- Final commit: 7556a218ec6b6bb8443485ff47b51c9ebf844e0d

# Pre-existing changes

- Exact status output: `git status --short` showed pre-existing edits across config, docs, and assets, including `config/aimbody.json` and `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`; this review changed none of those files.
- Files not created or modified by this session: all files in the pre-existing status output.

# Requested behavior

Determine if the opening ragdoll arm smoothing note is sufficiently specified for implementation, and identify what details would make it actionable.

# Specification alignment

- Current specification paths: `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`; `config/aimbody.json`; `docs/architecture/live-development/ragdoll-live-network.md`.
- Exact requirements: Opening note lines 1-14 asks for editable smoothing while ragdoll arms aim up/down/left/right, so quick look changes do not snap. Sections 14-17 specify physical head look and camera-forward arm extension; §15 allows camera presentation smoothing.
- Why the change follows the specification: No behavior change was made. Review found the request's desired feel is clear but its target, scope, response, and acceptance contract are not.
- Conflicts or decisions: Distinguish arm aim tracking from smoothed camera presentation, hybrid animation following, and the existing RMB right-arm precision correction. Decide which ragdoll modes and arms are covered, target frame, and tunable response behavior.

# Exact implementation changes

No implementation or specification changes. This changelog is the only file created by this session.

# Diagnostics

- Owner/category: Reviewed `AimBodyConfig`, `player-animation.cpp`, `physical-aim.h`, and `RagdollModeSystem` ownership guidance.
- Evidence: `aimbody.json` currently uses `mode: hybrid`, `arms_mode: hybrid`, `smoothingFactor: 1.0`, and an RMB-only right-arm pointing block with `follow_damping: 10.0`. These settings cover distinct behaviors and do not establish the requested two-arm ragdoll aim contract.

# Validation

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md` — review complete; missing decisions identified.
- Tests and exact commands: None; this was a specification review and the user did not ask for testing.
- Build status: Not run; no code changed.
- Runtime or hot-reload evidence: Not collected; no runtime change was requested.

# Human acceptance

- Visual review: Not performed.
- Gameplay review: Not performed.
- Still unverified: Actual in-game smoothness and tuning values; both require a defined contract and runtime play review.
