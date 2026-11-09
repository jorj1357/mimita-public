# Build-speed regression occurrence and gold reference

- Timestamp: 2026-11-09 13:29:15 EST (2026-11-09T18:29:15Z)
- Branch: `checkpoint/pre-live-kernel-20261108`
- HEAD: `8dfb1a4c docs: record lane integration, containment, and runtime evidence`
- Pre-existing unrelated changes preserved: modified `config/analytics.json` and submodule `devscripts/blender-ai`.
- Result: appended the 2026-11-09 occurrence to the existing build-time regression and added a gold reference. No code, build, or runtime process was changed in this documentation-only task.

## Files changed

- `docs/regressions/performance/README.md`, Entry 11: updated status to show the all-processor/high-priority mitigation is committed and full-rebuild performance proof remains open; linked the gold reference.
- `docs/regressions/performance/build-time-REG-2026-11-08.md`, Occurrence 2 (lines 110-188): recorded the old four-job/below-normal policy, lane build evidence, correction commits, 33.66-second incremental build, user-reported recovery with DeepSeek V4.1 Flash, and the remaining causal proof gap.
- `docs/gold/2026-11-09-build-speed-and-diagnosis-reference.md`: documented one dev-loop authority, one active EXE slot, all logical processors, `HIGH_PRIORITY_CLASS`, incremental versus cold-build scope, evidence boundaries, and the repeatable comparison method.
- This changelog.

## Evidence and review

- Prior policy confirmed in commit `b7516a78`: `build.py` capped jobs at four; the dev-loop worker capped lane jobs and used `BELOW_NORMAL_PRIORITY_CLASS`. Commit `ed8787fe` pins High priority and reduces the memory wait threshold to 16 MB.
- The lane build process was observed progressing from 162 to 169 object files, then later had stale `building` state with no final result. Its outcome is recorded as unknown.
- The main dev-loop integration record `docs/changelog/2026-11-09/20261109_181852-lane-import-integration-build-speed.md` reports artifact `2026-11-09T18:11:10.953010Z`, 1 compiled source, 622 skipped, and 33.66 seconds. This proves a fast one-source incremental build, not a matched cold-build speedup. Runtime attempts were partial and some mode-start/map-sync failures remained.
- Read: `docs/ROUTER.md`, `docs/regressions/README.md`, `docs/regressions/performance/README.md`, the existing build-time regression, `docs/specs/performance/performance.md`, `docs/specs/dev-loop/build-time-observability.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/gold/2026-11-04-events-jsonl-only-improvement-loop.md`.
- Focused skills: `docs/skills/documentation-checker-v1.md` — references checked; `docs/skills/efficiency-checker-v1.md` — causal claims separated from measurements.
- Validation: changed reference targets exist; `git diff --check` passed. No tests or build were run because this task only added documentation.
- Documentation TODO scan found unrelated active notes: `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:17,34,97`; `docs/specs/gamemodes/juggernaut.md:107`; `docs/specs/gamemodes/juggernaut-duel.md:226`; and `docs/specs/weapons/melee/melee-weapons.md:239`. Suggested expansions are in the final response; none were edited.
- Human review: the user reported the workflow felt back in a good state. Full-rebuild performance after the policy correction remains unverified with a matched source/cache/toolchain comparison.
