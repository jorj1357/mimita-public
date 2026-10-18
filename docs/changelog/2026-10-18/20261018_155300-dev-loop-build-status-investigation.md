# Task

- Task ID: Dev-loop build status and build-script consolidation investigation
- Summary: Traced the notes at the bottom of the dev-loop specification to current build, dashboard, launch, and script ownership.
- Status: INVESTIGATION_ONLY
- Date, time, timezone: 2026-10-18T15:53:00Z (2026-10-18 11:53 EDT, America/New_York)
- Branch: 20261011zt
- Base commit: 702973f3506712aa6e37741b523b17a1dbbad8c8
- Final commit: unchanged

## Pre-existing changes

Preserved the existing working tree, including the user's edits to `docs/specs/dev-loop/dev-loop.md`, the Melee spec and source files, configuration, asset deletions, and the prior audit changelog. No existing file was edited by this investigation.

## Findings

1. **Build status exists, but is coarse.** The dashboard renders `Build: phase=...`, current build, latest successful build, and identity-match status from `.dev/state.json` (`devscripts/dev-loop.py`, `_dashboard_lines`). The daemon sets `build_status` to `building` before launching the compiler and streams compiler output to its own console. The dashboard does not expose per-file/compiler progress.
2. **The daemon cannot handle controls during its own build.** `run()` calls `run_build()` synchronously; `run_build()` waits while reading the compiler subprocess output. Therefore the daemon does not return to `process_control_requests()` until that build finishes. A dashboard `1` request made during this interval waits in the control queue. This is the direct source-level explanation for the reported launch delay when the dev-loop itself is building.
3. **The documented `1` behavior intentionally waits for freshness.** If the latest artifact is stale or missing, `build-or-launch` marks a launch request pending and waits for a successful current build before calling `restart_latest()`. This avoids launching an artifact built from older source. If an artifact is current, `1` launches it immediately.
4. **An external `build_agent.py` run is outside dev-loop build state.** The loop invokes `build.py build-only` directly, not `build_agent.py`. `build_agent.py` has its own lock, which does not represent or serialize the loop's direct invocation. Running both entry points can produce concurrent builds without a shared status/coordination contract. The loop watches `build_agent.py` as an input too, so edits to that wrapper can themselves mark the source generation stale.
5. **The listed Python files are not all redundant wrappers.** `build.py` is the incremental compiler/linker used by dev-loop and by the agent wrapper. `build_game_dll.py` is invoked from `build.py` for the hot-code DLL. `build_toolchain.py` is imported by build owners to resolve compiler/toolchain paths. Removing these would remove build implementation, not just alternate entry points. `build_agent.py` is a wrapper and plausible consolidation candidate after callers/docs and locking are migrated. `buildv2.py` is a legacy entry point still called by `setup_developer.py`; it cannot be deleted safely until that setup flow is migrated. Its contents include process-kill behavior, so it should not be used as the dev-loop's build implementation.
6. **File size is not the observed failure.** `devscripts/dev-loop.py` is 3,542 lines and 155,668 bytes (about 152 KiB), versus the user's approximate 3,453 lines/120 KiB. That is sizable and may make ownership harder to navigate, but it does not by itself prevent reasoning. Splitting it now would add coordination surface without fixing the synchronous-build control stall. The spec already says not to split solely because of file size; measure a concrete maintenance/performance problem first.

## Decision-ready proposal

- Make dev-loop the only supported user-facing build/launch entry point, while retaining `build.py`, `build_game_dll.py`, and `build_toolchain.py` as internal build components.
- First route `setup_developer.py` and documented workflows away from `buildv2.py`; then remove or turn `build_agent.py` into a thin compatibility forwarder only after repository-wide callers and build-lock behavior are reconciled.
- Move build execution to a dedicated worker so the daemon can keep servicing dashboard/control requests and updating state during compilation. Keep one build at a time and the existing generation/debounce behavior.
- Define `[1]` during a build explicitly. Recommended: if a healthy game is already running, preserve it and report that it is running the prior build; otherwise allow an explicit launch of the last successful artifact while the new build continues, visibly marked stale. When no old artifact exists, wait for the first successful build. This preserves identity clarity and prevents an unnoticed stale launch.
- Improve the dashboard with a clear pending/building/failed/current status, elapsed time, and last build output/error summary. Per-file percentage is only appropriate if the compiler can provide reliable progress; do not invent a percentage from elapsed time.
- After the behavior policy is agreed, update the dev-loop specification and implementation together, then validate using the existing dev-loop and its runtime/build identity evidence.

## Evidence and validation

- Read `docs/ROUTER.md`, the dev-loop specification, build/EXE operation guide, live-development architecture, task completion workflow, and focused spec review guidance.
- Traced current source at `devscripts/dev-loop.py` (`process_control_requests`, `run_build`, `run`, and `_dashboard_lines`), `build.py`, `build_agent.py`, `build_game_dll.py`, `build_toolchain.py`, `buildv2.py`, and the `setup_developer.py` caller.
- No build, test, dev-loop command, or runtime scenario was run. No implementation changed.
- Human decision remains open on whether `[1]` may launch the last successful stale build while a new build continues.
