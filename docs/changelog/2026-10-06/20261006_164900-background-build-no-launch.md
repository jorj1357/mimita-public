# Keep background dev-loop builds from launching a new client

Time (EST): `2026-10-06T16:48:17-04:00`
Branch: `afad20a-rebuild`

## Result

The dev-loop continues to watch source files and run incremental builds in the
background, but a successful watcher-triggered build no longer launches a new
published `.exe` or interrupts the current client.

An explicit `[1]` action still launches the current published build. When `[1]`
queues a stale/missing build, that one-shot manual request is retained through
the successful build and launches afterward.

## Root cause

`DevLoop.run_build()` used:

```python
should_launch = self.auto_restart or self.manual_launch_requested
```

Therefore any successful automatic file-watcher build launched a new client
when auto-start was enabled. The watcher itself was not the problem: it already
recursively observes `.c`, `.cc`, `.cpp`, `.h`, `.hh`, `.hpp`, `.rc`, and `.py`
files under `src` and `external/libjuice/src`, and `build.py build-only` handles
dependency-based skipping of unchanged translation units.

## Change

`devscripts/dev-loop.py:601-604` now uses:

```python
should_launch = self.manual_launch_requested
```

The surrounding comment explicitly defines watcher builds as background
compile/publish work and `[1]` as the launch action.

## Specification review

Result: `PASS_WITH_HUMAN_REVIEW`.

- Requested behavior: edits should build incrementally without replacing or
  launching the running client.
- Code path: `DevLoop.watch()` → `change_event` → `run_build()` → successful
  publish decision.
- Corrected behavior: automatic builds publish only; manual `[1]` may launch.
- No specification conflict found. Live runtime acceptance is still separate
  from Python/source validation.

## Documents reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/architecture/live-development/live-development.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/2026-09-27/dev-loop-stale-build-launch-REG.md`

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Source inspection confirmed recursive C++ watching and the `build.py
  build-only` incremental path remain unchanged.
- No C++ build was needed because this patch changes only the Python
  orchestrator.

## Human acceptance still needed

Restart the currently running dev-loop so it loads this Python change. Edit and
save a C++ file, then confirm the loop builds/publishes in the background while
the currently running client remains open and unchanged. Press `[1]` separately
to confirm the explicit launch path still works.
