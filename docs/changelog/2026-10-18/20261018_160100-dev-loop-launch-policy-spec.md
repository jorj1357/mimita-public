# Task

- Task ID: Specify immediate last-successful build launch and single dev-loop build path
- Summary: Updated the dev-loop specification with the user's decision for `[1]`, a concrete non-overlapping build ownership plan, and the current implementation gap.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-18T16:01:00Z (2026-10-18 12:01 EDT, America/New_York)
- Branch: 20261011zt
- Base commit: 702973f3506712aa6e37741b523b17a1dbbad8c8
- Final commit: unchanged

## Pre-existing changes

Preserved the user's prior modifications throughout the working tree. In particular, `docs/specs/dev-loop/dev-loop.md` was already modified and contained informal user notes, including the 2026-10-18 build-status and launch report at its bottom. No other pre-existing changes were edited for this task.

## Changes

### `docs/specs/dev-loop/dev-loop.md`

- Lines 98–102, §1: changed the desired outcome from blocking stale-artifact launch to explicitly allowing the last successful artifact to be launched with its source age visible.
- Lines 286–339, new §5.2 “One user-facing build path and script ownership”: established dev-loop as the supported user-facing build/status/launch path; distinguished compiler/DLL/toolchain components from removable wrappers; gated `build_agent.py` and `buildv2.py` removal on caller/setup migration; specified a single daemon-owned worker, responsive control loop, checkout-wide build lock, and useful build status.
- Lines 359–367, §6: allowed explicit launch of the last successful artifact while a newer build is pending, running, or failed, with its identity and stale state shown; prohibited silent automatic rollback.
- Lines 373–412, §7: specified `[1]` launches/reuses the last successful valid build immediately while a newer build continues; handles no-artifact startup; preserves identity/readiness checks and allows replacement only under an explicitly enabled auto-restart policy.
- Lines 662–671, §14: documented that current code still queues and synchronously waits for a build, so the desired behavior is not implemented.
- Line 2041, §22.2: changed the dashboard key table to match the new `[1]` behavior.

Representative exact wording changed:

```text
Before: If no build exists or the latest build is stale, `[1]` queues a build. When that build succeeds, the loop may continue to launch it because the user made an explicit launch request.
After: [1] means “launch the last successful build now.” It is not a request to wait for an in-progress build to finish.
```

The added §5.2 includes this exact design rule:

```text
The intended implementation boundary is one daemon-owned build worker plus a responsive daemon control loop.
```

## Review and validation

- Read `docs/ROUTER.md`, the dev-loop specification, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/live-development/live-development.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/documentation-checker-v1.md`.
- Focused spec review: PASS for expressing the user's decision and separating target behavior from current behavior; runtime acceptance remains outstanding because code was not changed.
- Documentation review: checked section placement, cross-references, controls table, and contradictions with §6/§14. The existing TODO at `docs/specs/dev-loop/dev-loop.md:1` says `2026 10 17 1236 jorj todo make htis doc better`; this task adds the requested focused clarification but does not claim the whole document has been reorganized.
- `git diff --check` reported trailing whitespace only on the user's pre-existing informal note lines at the bottom of the already-modified specification; those historical notes were preserved.
- No code, tests, build, dev-loop command, or runtime scenario was run. Only the specification was edited; the implementation change remains future work.
- No regression record was added because no runtime regression was newly reproduced in this documentation-only task.
