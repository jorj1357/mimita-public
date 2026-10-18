# Task

- Task ID: Implement dev-loop build and launch behavior
- Summary: Implemented the specified immediate `[1]` launch, asynchronous single-worker builds, correlated build requests, and checkout-wide compiler serialization.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-18T16:19:00Z (2026-10-18 12:19 EDT, America/New_York)
- Branch: 20261011zt
- Base commit: 702973f3506712aa6e37741b523b17a1dbbad8c8
- Final commit: unchanged

## Pre-existing changes

Preserved the user's unrelated edits across the Melee mode, assets, animation, networking, and source tree, as well as earlier edits to `docs/specs/dev-loop/dev-loop.md` and its specification changelog. This implementation task changed only the build/dev-loop files listed below and this changelog.

## Changes with exact before and after text

### `devscripts/dev-loop.py`

- Lines 1443–1466, `queue_build_request`: before, there was no API build-request mode validation/correlation and requests could only use the one synchronous build path. After, `queue_build_request(request_id, mode="debug")` accepts debug/release, attaches same-generation/same-mode requests to an active job, queues other requests, and tracks each request's status. Exact new signature: `def queue_build_request(self, request_id: str, mode: str = "debug") -> bool:`.
- Lines 1614–1664, `start_build_worker`: before, the main daemon called `run_build()` synchronously. After, it starts one worker thread, selects a queued mode and request group, and returns to the responsive control loop. Exact new worker invocation: `args=(requested_generation, request_ids, requested_mode)`.
- Lines 1666–1731, `run_build` and `drain_build_completions`: before, the build path blocked control processing and completion could not be independently correlated. After, the worker streams bounded progress and publishes a completion record; the daemon thread updates artifact/build state and request results, with queued generations serialized behind the active job.
- Lines 1490–1538, control handling: before, there was no `build` control action and `[1]` could wait for a build. After, explicit build requests enter the queue, while launch actions call `launch_latest()` as soon as an artifact exists and only wait when no successful artifact exists.
- Lines 2099 onward, `launch_latest`: permits the latest successful published artifact to be selected while stale, preserves server/client identity checks, and returns launch success. The state comparison now checks the running pair's own IDs rather than treating a newer artifact publication as a pair mismatch.
- Lines 142–148, watched build inputs: removed the compatibility wrapper from game-source build triggers and added `build_lock.py`, so edits to the shared lock are observed.
- Lines 987–996, pair identity status: reports `server_client_build_identity` when executable paths match but the recorded pair identities do not.
- Lines 2533 and 3340: before, controls were labeled `[1] build/launch`; after, both dashboard/status labels say `[1] launch latest`.
- Lines 3795 onward: added `--build-mode debug|release` for correlated build requests. Added build wait/status fields to the daemon state.

### `build.py`

- Lines 8–10 and 32–42: before, the script described `build_agent.py` as the usual caller and had no shared checkout lock. After, it is documented as an internal compiler pipeline and acquires the shared advisory lock; lock contention exits with code 3. The nested DLL build receives a parent PID marker so it can share the parent's lock without deadlocking.

### `build_game_dll.py`

- Lines 12–29: before, standalone DLL compilation did not share the executable build's lock. After, it acquires the same checkout lock when invoked independently and recognizes the verified parent marker when invoked within `build.py`.

### `build_lock.py`

- New file; there was no previous shared lock module. Adds an OS advisory lock with owner metadata, explicit contention errors, and parent-process validation. OS lock ownership releases on process exit, avoiding stale-file lock recovery races.

### `build_agent.py`

- Lines 1–32: before, this file owned a separate lock, ran `build.py` directly, and wrote its own build history. After, it is a compatibility wrapper submitting a correlated debug/release build request to the dev-loop and waiting for its result.

### `buildv2.py`

- Lines 1–16 and 82–108: before, this entry point force-killed `mimita.exe`, deleted the executable, and invoked `build.py` itself. After, it preserves its submodule/toolchain setup and routes the request through the dev-loop with build mode and timeout; it does not terminate game processes or delete an executable.

### `setup_developer.py`

- Lines 161–167: before, setup invoked `buildv2.py timeout 356400`. After, it submits a long-wait build request directly to `devscripts/dev-loop.py`.

### `docs/operations/build-and-exe/build-and-exe.md`

- Lines 7–26: before, it called `build_agent.py` and `buildv3.py` parallel build entries and retained obsolete cold-build guidance. After, it names dev-loop as the build/status/publish/launch authority and documents immediate last-successful `[1]` launch plus shared compiler locking.
- Lines 77–87 and 118 onward: before, instructions directed contributors to separate build wrappers; after, the canonical request is `python devscripts/dev-loop.py --request build --wait-for-build`, with release mode explicitly selectable.

## Reasoning and scope

This aligns runtime ownership with the already-updated specification: compilation no longer blocks key/control handling, `[1]` means launch the last successful artifact immediately, automatic build completion publishes but does not replace the running game, and legacy build wrappers cannot create a second build pipeline. The compiler and hot-DLL components remain because the spec identifies them as internal build owners. Build requests support debug and release modes so compatibility behavior is retained.

## Documents and focused review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/live-development/live-development.md`, the routed runtime/build/logging/terminal documents, and `docs/skills/spec-behavior-review-v1.md`.
- No new regression record was added: this turn implemented the documented behavior; it did not reproduce a new gameplay regression.

## Validation and remaining human review

- `python -m py_compile devscripts/dev-loop.py build_agent.py buildv2.py setup_developer.py build.py build_game_dll.py build_lock.py`: PASS.
- `git diff --check`: no new whitespace failures in the implementation files. It reports pre-existing trailing whitespace in user-authored notes at the bottom of the modified dev-loop spec, Melee spec, and source comment notes; those lines were preserved.
- Read-only `python devscripts/dev-loop.py --status --json`: the currently running daemon is PID 29408 with the old script hash, and the remaining client PID 25252 is alive. It reports the server PID 17528 is no longer alive and the old daemon records `unknown AI action: build`. This confirms the current daemon has not loaded the new implementation. No build or launch action was sent, and the client was left untouched.
- No C++ build, tests, runtime scenario, live `[1]` acceptance, or visual/human review was performed. The always-on watcher defers replacing the daemon while a live client run exists; live acceptance remains pending until the current run ends and the watcher can load the changed Python code safely.
