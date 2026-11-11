# Task

- Task ID: Expand hot-reload implementation plan
- Summary: Add a concrete first implementation runbook for the existing-repository, NPC-first DLL migration and distinguish the pilot-start evidence gate from full Phase 0 completion.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-11T21:41:01Z; 2026-11-11 16:41:01 EST
- Branch: `checkpoint/pre-live-kernel-20261108`
- Base commit: `9bfbaeff`
- Final commit: `9bfbaeff` (no commit created)

# Pre-existing changes

- Exact status output: `git status --short -- docs/specs/hotreload/hotreload.md` reported `M docs/specs/hotreload/hotreload.md`. Full `git status --short` fails while Git inspects the `external/libjuice` submodule: permission denied at `.git/modules/external/libjuice`.
- Files not created or modified by this session: The target spec already contained uncommitted user-authored material before this session, beginning with `2026 11 11 1507 jorj`, the quoted five-point direction, and sections 1–8 describing the desired runtime/game architecture. That material was read and preserved. Other repository status is unknown because of the submodule error.

# Requested behavior

Expand `docs/specs/hotreload/hotreload.md` with a detailed, understandable plan to reach save-C++ → live `game.dll` activation, using the existing repository and NPC behavior as the first proof. The user selected the current repository and NPC behavior in response to the optional choices. Preserve current world loading during the first proof as the scoped plan assumption.

# Specification alignment

- Current specification paths: `docs/specs/hotreload/hotreload.md`; `docs/ROUTER.md`; `docs/specs/dev-loop/agent-lanes-build-authority.md`; `docs/architecture/live-development/live-development.md`; `docs/architecture/live-development/hot-kernel.md`; `docs/architecture/live-development/module-migration-plan.md`; `docs/architecture/live-development/self-contained-workspace.md`; `docs/architecture/time-and-formatting/time-and-formatting.md`; `docs/skills/spec-behavior-review-v1.md`; `docs/skills/documentation-checker-v1.md`; `docs/skills/logging-checker-v1.md`; `docs/operations/task-completion/task-completion.md`.
- Exact requirements: The hot-reload spec requires a resident runtime and replaceable game DLL, an NPC proof retaining live state, automatic save-triggered candidate builds, safe activation/rollback, and eventual default ownership of eligible implementation `.cpp` files by `game.dll`.
- Why the change follows the specification: The new runbook makes the first implementation steps, current code touchpoints, state/ABI rules, failure behavior, proof ladder, and source-migration sequence concrete. Phase 1 may start after slice-specific evidence; full Phase 0 still requires the complete inventory and controlled measurements.
- Conflicts or decisions: `module-migration-plan.md` still lists map-editor-first work in its immediate continuation order. The user selected an NPC-first pilot for this hot-reload plan; reconcile the separate migration plan before scheduling that broader plan. No second repository is proposed. Existing world loading stays in place for the first NPC proof.

# Exact implementation changes

## File: `docs/specs/hotreload/hotreload.md`

- Lines/functions/headings: New `3.1 Concrete first-implementation runbook` at line 90; steps 0–7 at lines 94–200; Phase 0 pilot-start and full-exit gates at lines 202–211; Phase 2 title and watcher wording at lines 222–226.
- Old content or behavior: Section 3 described the coordinator and lifecycle generally. Phase 0 required the complete inventory before Phase 1, without a separate minimum evidence gate for beginning a bounded pilot. Phase 2 said to implement the watcher after Phase 1, despite the desired first live-edit proof needing an automatic candidate path.
- New content or behavior: Added exact existing repository touchpoints (`build_game_dll.py`, `GameAPI`, `HotReloadSystem`, engine tick integration, ownership manifest, source inventory), runtime data lifetime clarification, minimum C ABI and NPC behavior adapter steps, asynchronous candidate-build and safe-switch sequence, same-process proof ladder, and batch migration to all eligible game sources. Added a pilot-start gate distinct from full Phase 0 completion. Clarified that Phase 2 generalizes the pilot coordinator.
- Reason: Turn the architectural goal into a sequence that starts with one visible NPC edit without requiring a complete audit of all source files first.
- Why unrelated behavior is preserved: No implementation source, build target, runtime process, or historical module evidence was changed. The existing spec content and map/world loading remain intact.

# Diagnostics

- Owner/category: Documentation/spec review.
- Input: Current hot-reload spec, implementation paths named by the spec and source inventory, live-development and hot-kernel architecture, lane build authority, and focused documentation/spec/logging review checklists.
- Decision: Add a bounded implementation runbook; do not claim the target is already implemented.
- Output: Concrete NPC-first implementation sequence and acceptance proof ladder.
- Failure or rejection reason: Full Git status could not read embedded libjuice metadata. No file-write failure occurred in this session.
- Rate limiting: Not applicable.

# Validation

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md` — requirements and existing architecture reviewed; `docs/skills/documentation-checker-v1.md` — headings, scope, and doc consistency reviewed; `docs/skills/logging-checker-v1.md` — required runtime evidence points included.
- Tests and exact commands: No tests run (documentation-only change). `git diff --check -- docs/specs/hotreload/hotreload.md` completed with no output after removing trailing whitespace from the informal `do this all` line. `rg` verified the new section, steps, and Phase 0 gates.
- Build status: Not applicable; no code changed and no build was run.
- Runtime or hot-reload evidence: None; this is a plan, not an implementation proof.
- Output files: `docs/specs/hotreload/hotreload.md`.

# Measured evidence

- Before values: No repo disk-usage measurement was available in the previous turn.
- After values: `Get-PSDrive C` reported 46,164,758,528 bytes free and 464,723,484,672 bytes used (about 43.0 GiB free, 432.7 GiB used). This is drive-wide capacity, not a repo folder breakdown.
- Timestamps: Disk reading 2026-11-11T21:38Z. Spec validation 2026-11-11T21:41Z.
- Tick/frame/network measurements: Not collected; no runtime change occurred.

# Regression review

- Regression entry appended: no
- Why this is or is not a confirmed regression: Documentation-only plan change; no behavior regression occurred.
- Related regression paths: None.

# Human acceptance

- Visual review: Not applicable.
- Gameplay review: Not performed; planned acceptance is in the NPC proof ladder.
- Multiplayer review: Not performed; remains a later phase.
- Still unverified: Whether the planned ABI and NPC adapter can be implemented without additional ownership changes; actual build and activation latency; state continuity; callback/job quiescence; migration and rollback; coordination with the map-editor-first order in `module-migration-plan.md`.

# Related feature record

- Feature path: `docs/specs/hotreload/hotreload.md`
