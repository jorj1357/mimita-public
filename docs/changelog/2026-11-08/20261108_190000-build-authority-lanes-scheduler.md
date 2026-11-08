# Build authority, machine-wide scheduler, and mandatory agent lanes

## Changed

- Added `devscripts/build_authority.py`. `build.py` now refuses to compile
  unless the dev-loop authority token, a dev-loop parent/marker, or the human
  `MIMITA_BUILD_OVERRIDE=1` is present. `module-build-worker.py` accepts only
  the game/ModuleHost path or a human override.
- Added `devscripts/build_scheduler.py` with machine-wide slots: one EXE build
  (`exe-0`) and two module builds (`module-0`, `module-1`). Builds wait for a
  slot instead of failing, coalesce newer edits, reclaim stale slot owners,
  and report wait reasons. It also waits when free physical memory is below
  `MIMITA_BUILD_MIN_FREE_MB` (default 1024).
- `build.py` acquires the EXE slot, caps default compiler jobs at 4 on this
  machine, and no longer rebuilds every module with
  `module-build-worker.py --all`. Module DLLs are built on demand by the
  running game's `ModuleHost`.
- `devscripts/dev_loop/build.py` writes the EXE authority token, sets
  `MIMITA_DEV_LOOP_BUILD`, passes the lane ID, caps lane jobs at 4, and runs the
  build below normal priority.
- `module-build-worker.py` acquires a machine-wide module slot before compiling
  and releases it on success, failure, or cancellation.
- `devscripts/agent-workflow.py` now refuses `--mode 1 build` and adds
  `--mode 1 ensure`. `devscripts/agent-lane.py` adds `ensure`, refuses `build`,
  sets `MIMITA_LANE_ID`, writes a default 4-job lane config, and adds `list` to
  show lane dev-loop PIDs and liveness.
- Dev-loop state/run metadata now records `lane_id`, `build_authority`, and
  `build_policy`.
- `network-policy` snapshot policy keeps the live module decision but defaults
  safely: `resendPrevious` is always enabled; queue pressure may reduce
  duplicate copies but never disables the previous-snapshot resend.
- Added `docs/specs/dev-loop/agent-lanes-build-authority.md` and updated
  `AGENTS.md`, the router, dev-loop, hot-reload, live-development, build/EXE,
  task-completion, runtime-scenario, and multi-client workflow docs.

## Evidence

- `python -m py_compile` passes for `build.py`, `build_authority.py`,
  `build_scheduler.py`, `module-build-worker.py`, `dev-loop.py`,
  `dev_loop/build.py`, `agent-lane.py`, `agent-workflow.py`, and
  `module_ownership.py`.
- Module ownership audit: PASS, `module_sources=9 exe_sources=579`.
- Scheduler cap test: two module slots acquired, third failed within timeout;
  one EXE slot acquired, second failed within timeout; all released.
- Authority check from an agent shell: `exe -> missing_authority`,
  `module -> missing_authority`.
- Agent refusal: `agent-workflow.py --mode 1 build` and
  `agent-lane.py build` exit 2 with the dev-loop-only message.
- Dev-loop build after the change: `build_succeeded`, source generation 69,
  published artifact `2026-11-08T18:57:41Z` (it was a no-op incremental build).
- Existing live runs still show `chunk-collision` generation 31 handling contact
  solves and `network-policy` generation 13 activating in the same server/client
  PIDs from the earlier live-edit trial.
- `agent-lane.py list` found four pre-existing lane dev-loops alive (one per
  lane worktree). They are now subject to the machine-wide scheduler; idle lanes
  can be stopped with `cleanup` while preserving evidence.

## Limits

- The running dev-loop daemon predates the authority-token writer. Its builds
  currently pass through the python-parent fallback; the watchdog defers daemon
  replacement while a live run exists. New lanes and restarted daemons use the
  explicit token path.
- No new cold EXE artifact was forced for this Python/tooling change.
- Module builds were not re-run after the scheduler change; the running game's
  next module edit will exercise that path.
- No gameplay, reconnect, 60 Hz, or frame-rate acceptance is claimed.
- Concurrent edits in `open-gameplay-issues.md`, `multiplayer-*`, `server-*`,
  `npc.cpp`, and the ICE reconnect changelog are not part of this session.

## Review

Routing, live-development, dev-loop, hot-reload, logging, and task-completion
rules were consulted. This change is build/tooling authority, not gameplay
behavior.
