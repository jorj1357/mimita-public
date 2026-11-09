# Build priority, full jobs, and headless helper windows

- Timestamp: 2026-11-09 12:36:44 EST (2026-11-09T17:36:44Z)
- Branch: `checkpoint/pre-live-kernel-20261108`
- HEAD before this work: `5af24993 build: enforce repository-local lane and build paths`
- Pre-existing unrelated change preserved: modified submodule `devscripts/blender-ai`.
- Result: source and documentation updated; no build was launched by this agent.

## Changes

- `build.py`, `_MAX_BUILD_JOBS` / `BUILD_JOBS` (lines 100-103): replaced the four-job/half-processor cap with all logical processors and removed the environment-variable override. The default full build now uses `os.cpu_count()` jobs.
- `devscripts/dev_loop/build.py`, `run_build` (lines 39-48): removed the per-lane four-job clamp and fixed four-job lane-config override; the Windows child starts with `HIGH_PRIORITY_CLASS` while retaining the machine-wide single active EXE slot.
- `devscripts/agent-lane.py`, `create_lane` (former lane-config block after metadata write): removed creation of the obsolete `agent-lane-build.json` four-job cap.
- `build.py`, hot reload DLL build launch (lines 429-436): added Windows `CREATE_NO_WINDOW` so the helper no longer opens a terminal. Compiler tools already use `CREATE_NO_WINDOW` through `build_toolchain.run_tool`.
- Updated `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/dev-loop/dev-loop.md`, and `docs/operations/build-and-exe/build-and-exe.md` to document one active EXE build, all logical processors, high Windows process priority, and headless helper launches.

## Evidence and validation

- Read: `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/dev-loop/build-time-observability.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/architecture/live-development/live-development.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- Focused skills: none applicable to this build-policy code/doc update.
- Source validation: `python -m py_compile build.py devscripts/dev_loop/build.py devscripts/agent-lane.py` passed; `git diff --check` passed.
- Build evidence: not run. Agent build authority prohibits agents from building; no new EXE build was started.
- Runtime evidence: read-only scheduler status showed `exe-0` owned by lane build PID 33988. The root checkout build PID 31964 had no compiler children and was waiting on the slot. Active build PID 33988 and its compiler descendants were changed from BelowNormal to High priority; queued PID 31964 was also changed to High. No processes were stopped.
- Human review: terminal-window behavior and the next build's full-job/high-priority behavior still need observation after a fresh dev-loop worker loads the changed code. The active build began before this change and retains its original job count.

## Review set

- `docs/specs/dev-loop/agent-lanes-build-authority.md`
- `docs/specs/dev-loop/build-time-observability.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/architecture/live-development/live-development.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/operations/task-completion/task-completion.md`
