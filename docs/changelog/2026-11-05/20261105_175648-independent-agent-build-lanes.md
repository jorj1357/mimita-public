# Independent agent build lanes

- Time: 2026-11-05 17:56:48 EST (2026-11-05T22:56:48Z)
- Branch: `20261011zt`
- HEAD at start: `a992f9a4ebb165eb403e6892f953e8adc87dc7c6`
- Commits created: none
- Result: `PASS_WITH_HUMAN_REVIEW`

## Pre-existing worktree state

The coordinator checkout contained extensive pre-existing modified and
untracked files, including `devscripts/dev-loop.py`, its CLI/GUI, several
specifications, gameplay sources, assets, and prior changelogs. These edits
were preserved. This session added only the lane manager/source identity/build
metadata changes and the lane/build documentation described below.

An old prunable worktree registration pointed at a missing temp checkout. It
was removed by `git worktree prune` after temporary lane validation. No files
for that stale worktree existed to remove.

## Changes

- Added `devscripts/agent-lane.py`: named worktree creation, status, focused
  checks, per-lane build/run, safe process cleanup that preserves evidence, and
  reviewed-commit integration assembly. Worktrees live beside the coordinator
  checkout. Required submodule source is copied from the matching coordinator
  revision or initialized for the lane; a matching local Manifold prebuilt is
  copied into the lane.
- Added `devscripts/dev_loop/source_identity.py` and updated the build worker
  and publisher to record the Git revision, dirty state, source fingerprint,
  submodule revisions, Manifold library hash, and whether the source changed
  during compilation. Each lane can set its compiler concurrency limit in its
  own `.dev/agent-lane-build.json`.
- Added stable promotion for an explicitly reviewed, clean integration tree.
  Promotion copies the verified immutable artifact to `.dev/stable-builds/`
  and atomically updates `.dev/stable-build.json`; it does not replace a live
  executable.
- Replaced the dev-loop spec's future shared-compiler lane proposal with
  isolated worktree lanes. Updated the one-build rule to apply per checkout and
  clarified that independent worktrees may compile concurrently. Added the
  worktree boundary to the multi-instance spec and documented lane commands in
  the build procedure. Corrected its artifact description to match the actual
  dev-loop publication path and lane launch workflow.

## Documents followed

Read `AGENTS.md`, `docs/ROUTER.md`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/architecture/live-development/live-development.md`,
`docs/skills/documentation-checker-v1.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/specs/dev-loop/dev-loop.md`,
`docs/specs/dev-loop/quality-gates.md`,
`docs/specs/dev-loop/multi-instance.md`,
`docs/specs/debug-logging/canonical-jsonl.md`,
`docs/workflows/runtime-scenario-validation.md`, and
`docs/architecture/time-and-formatting/time-and-formatting.md`.

## Validation evidence

- `python -m py_compile devscripts/agent-lane.py devscripts/dev_loop/source_identity.py devscripts/dev_loop/build.py devscripts/dev-loop.py` — PASS.
- `python devscripts/agent-lane.py --help` and `build --help` — PASS; lane and
  compiler-job interfaces are visible.
- Created two temporary worktrees from the same commit. Both reported distinct
  branches and clean source identities; focused checks confirmed isolation. A
  Python syntax probe in one lane selected only `python-syntax`; the other lane
  remained clean. The probe and temporary worktrees/branches were removed.
- Created a fresh lane after dependency setup was added. It populated the
  required submodule inputs, copied the Manifold prebuilt library, remained
  Git-clean, and passed lane status/check. Its temporary worktree and branch
  were removed.
- `git diff --check` on the touched tracked documentation/build files — PASS.
- Documentation Checker v1 — PASS_WITH_FINDINGS. Updated build artifact wording
  to match the implementation and verified the new cross-references. Existing
  dev-loop TODOs remain unchanged: `docs/specs/dev-loop/dev-loop.md:17` says
  `2026 10 17 1236 jorj todo make htis doc better` (suggestion: outline the
  build, launch, agent-control, evidence, and lifecycle sections with owner
  links); line 61 says `2026 10 13 1915 jorj todo -` (suggestion: identify the
  missing dev-loop behavior or resolve/remove the empty note); line 2385 says
  `2026 10 18 1146 jorj todo problmes` (suggestion: name the concrete problem
  and expected outcome). The existing `## Independent agent worktree lanes`
  section and build procedure also lack explicit purpose/does-not/audience
  headers; recommend a later focused documentation cleanup.
- Specification and Behavior Review v1 — PASS_WITH_HUMAN_REVIEW. Reconciled the
  old single-compiler lane proposal and one-build wording to be per checkout;
  there is no gameplay behavior change. Actual full-build and runtime acceptance
  remains open below.
- No game build or runtime test was run. The implementation changes are in the
  Python lane/build orchestration and documentation; actual concurrent full C++
  compiles, runtime launch/journal separation, and stable promotion still need
  acceptance on committed implementation code.

## Focused review and remaining acceptance

No additional focused Markdown skill was required by the build/dev-loop route.
There is no gameplay behavior change, so no regression record was added.
Before relying on the workflow for production agent tasks, commit this lane
manager and run two real lane builds concurrently with a resource-appropriate
`--jobs` value. Then launch each lane and verify distinct build identities,
processes, run manifests, and per-process journals. Finally promote a reviewed
integration commit and verify the stable pointer and executable identity.
