# Agent build mode documentation

**Time:** 2026-11-05 18:54 EST  
**Branch:** current checkout branch (not recorded in this session)  
**Commits:** none

## Final state

Documented the current agent build modes and the future mode 3 direction. Mode
1 is the shared checkout with one build owner; mode 2 provides independent
agent worktrees; mode 3 is explicitly future work for save-triggered background
compilation and safe activation of replaceable code. The docs clarify that
incremental executable builds already detect and coalesce saved changes, while
the current C++ hot DLL supports only a narrow source set and invokes its build
synchronously. Every C++ edit is a long-term target, not a current guarantee.

## Files changed

- `docs/specs/dev-loop/dev-loop.md`: expanded the three-mode explanation,
  save/build behavior, lane ownership, compile-versus-reload boundary, and
  integration step.
- `docs/operations/build-and-exe/build-and-exe.md`: added current watcher and
  mode 3 status guidance.
- `docs/architecture/live-development/live-development.md`: distinguished
  artifact publication from changing a running process and described future
  safe module activation.
- `docs/specs/hotreload/hotreload.md`: replaced the stale TODO preamble with
  current C++ reload facts and mode 3 goals; retained the JSON reload detail.
- `docs/ROUTER.md`: changed live-code routes from a missing feature page to
  existing architecture and hot-reload documentation.
- `docs/architecture/live-development/hot-kernel.md` and
  `docs/architecture/live-development/hot-audio-contract.md`: replaced active
  references to the missing feature page with existing authority documents.

Pre-existing user edits elsewhere in the checkout were preserved.

## Documents and review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/dev-loop/dev-loop.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/architecture/live-development/live-development.md`,
  `docs/specs/hotreload/hotreload.md`, the time/filename standard, and the
  documentation checker skill.
- Cross-checked current watcher, build publication, and focused DLL source set
  against `devscripts/dev-loop.py`, `devscripts/dev_loop/build.py`,
  `build_game_dll.py`, and `src/hot-reload/hot-reload-system.cpp`.

## Validation

- `git diff --check` on changed documentation — passed.
- Checked the updated router paths and new internal documentation links against
  existing files — passed.
- No code changed and no build or runtime test was needed. Mode 3 remains a
  documented direction and was not implemented or runtime-validated.
