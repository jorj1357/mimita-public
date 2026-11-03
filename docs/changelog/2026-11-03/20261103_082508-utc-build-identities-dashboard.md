# UTC build identities and dev-loop dashboard

- **Local time:** 2026-11-03 08:25:08 EST
- **Branch:** `20261011zt`
- **HEAD observed during work:** `45d82800ac923a105305facb96b155fc67c7f9f3` (`v1 of the dev loop pthon script aweosme`). HEAD advanced during this task; this agent did not create that commit. It contains the principal implementation and documentation changes listed below.
- **Pre-existing work:** The worktree initially contained unrelated user changes. At completion, `devscripts/blender-ai` remains modified and was preserved. The only tracked-file diff from the observed HEAD is a small follow-up in `devscripts/dev-loop.py` that reports the active runtime's full build ID consistently.

## Changes

- `devscripts/dev-loop.py`: new published artifacts use UTC ISO 8601 timestamp IDs with microseconds, colon-free timestamp folder names, collision advancement, and SHA-256-bound build IDs. Discovery and retention still recognize legacy numbered folders. Runtime metadata and dashboard status distinguish the latest successful artifact from the source generation currently being built, and expose the exact active runtime build ID. Scenario status derives verdict, build identity, and canonical journal path from existing runner journal records.
- `devscripts/dev_loop_gui.py`: adds a native Tk dashboard with selected build/freshness, build phase and recent output, runtime readiness, scenario evidence, and daemon controls.
- `docs/specs/dev-loop/dev-loop.md`, `docs/operations/build-and-exe/build-and-exe.md`, and `docs/architecture/time-and-formatting/time-and-formatting.md`: document timestamp IDs, SHA identity, legacy discovery, and the dashboard/status contract.

## Evidence

- `python -m py_compile devscripts/dev-loop.py devscripts/dev_loop_gui.py`: passed.
- `git diff --check`: passed.
- Isolated checks during implementation covered timestamp format and ordering, same-time collision handling, timestamp artifacts sorting ahead of numbered legacy artifacts, and scenario verdict/build/journal parsing: passed.
- `python devscripts/quality/agent-gate.py --no-build --skip-scenarios`: passed build-independent checks and selftests against the existing executable. This did not run a new build or scenarios.
- The existing game and daemon remained active on legacy build 251. The watcher deferred source reload because a live run was present. No new timestamped artifact was published, no full build/runtime gate was run, and live scenario or human gameplay acceptance is not claimed.
- The dashboard was opened and visually inspected while attached to the existing daemon. Runtime displayed the existing legacy build; new daemon-side timestamp behavior awaits a safe restart/build cycle.

## Open documentation notes

The repository-wide TODO scan found existing unrelated notes at `docs/specs/weapons/tool-weapon-ideas.md:1`, `docs/specs/visuals/visuals.md:1`, `docs/specs/actor-presets/actor-presets.md:264`, `docs/specs/gamemodes/gamemodes.md:440`, `docs/architecture/player-npc-systems/npc-movement.md:22`, `docs/specs/terminal/terminal.md:3`, `docs/specs/weapons/weapons.md:74`. These pre-existing notes were left untouched; each needs its nearby concept clarified or the historical TODO removed after review.
