# buildv3 auto-launch

- Time: 2026-09-16T22:59:31.208Z (America/New_York display context)
- Branch: `8292026stash`
- Result: PASS_WITH_HUMAN_REVIEW
- Pre-existing edits: preserved; `git status` already showed unrelated changes in config, source, docs, and regression files.

## Change

- File: `buildv3.py`, `__main__` entrypoint.
- Old behavior: delegated to `build_agent.main()` and stopped after the build.
- New behavior: catches a successful `SystemExit`, reads `build/build-result.json`, and launches the recorded executable with `subprocess.Popen` from its directory. `SUCCESS` and `NOTHING_CHANGED` launch; failed builds, missing result data, or missing executables do not launch.
- Header contract was updated to state that the newly built executable is launched, while running executables are not killed or closed.

## Documents and focused review

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/terminal-command-checker-v1.md`
- Terminal-command review: PASS for one owned human build entrypoint and explicit success/missing-file guards.

## Validation

- `python -m py_compile buildv3.py`: PASS.
- A full cold build and visual/runtime launch were not run in this session; human review should run `python buildv3.py` and confirm the timestamped executable opens.
