# Agent workflow modes

**Time:** 2026-11-05 18:19 EST  
**Branch:** current checkout branch (not recorded in this session)  
**Commits:** none

## Final state

Added explicit workflow mode 1 and mode 2. Mode 1 is the default and continues
to route shared-checkout build, run, and status operations through the existing
dev-loop. Mode 2 delegates lane commands to `devscripts/agent-lane.py`.

Mode 2 now makes a temporary Git snapshot of the coordinator's tracked and
non-ignored untracked work before creating a lane. The snapshot uses a unique
temporary index and does not change the coordinator's real index. Lane compiler
jobs default to one. Lane builds use shared ccache storage with the lane-root
as `CCACHE_BASEDIR`; objects, PCH, executables, dev-loop state, and journals
remain per worktree.

## Files and reasoning

- `devscripts/agent-workflow.py`: added the user-facing `--mode 1|2` dispatcher;
  mode 1 stays the fallback, and mode 2 delegates to the lane manager.
- `devscripts/agent-lane.py`: default lane creation now snapshots the current
  coordinator worktree; compiler parallelism defaults to one.
- `devscripts/dev_loop/build.py`: mode 2 lane builds share ccache storage while
  retaining independent incremental outputs.
- `docs/specs/dev-loop/dev-loop.md` and
  `docs/operations/build-and-exe/build-and-exe.md`: documented the two modes,
  baseline snapshot, lane evidence, resource limits, and stable promotion.

Before this session, these files already contained substantial uncommitted
work from prior sessions and the user. Those existing edits were preserved;
this record describes only the mode selector, snapshot, cache, defaults, and
documentation changes made in this session.

## Documents and focused review

- Read `AGENTS.md`, `docs/ROUTER.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/live-development/live-development.md`, and
  `docs/skills/documentation-checker-v1.md`.
- Reviewed the mode descriptions and command examples for consistency.

## Validation

- `python -m py_compile devscripts/agent-lane.py devscripts/agent-workflow.py devscripts/dev_loop/build.py` — passed.
- `python devscripts/agent-workflow.py --help` — passed.
- `python devscripts/agent-workflow.py --mode 2 build demo --help` — passed and dispatched to the lane build help.
- `python devscripts/agent-workflow.py --mode 1 check` — correctly rejected because focused changed-file checks are mode 2 only.
- No C++ build, concurrent lane build, ccache hit-rate check, or game runtime validation was performed. These are required to prove performance and end-to-end behavior on this machine.
- Human review should confirm whether the mode 1/2 command wording is clear and choose whether mode 2 becomes the normal default later.
