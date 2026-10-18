# Task

- Task ID: Make the dev-loop entry point self-ensuring and dashboard input responsive
- Summary: Fixed a daemon startup crash in automatic builds, clarified the one-launch availability contract, and separated fast key polling/feedback from periodic rainbow/status refresh.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-18T16:26:08Z (2026-10-18 12:26:08 EDT, America/New_York)
- Branch: 20261011zt
- Base commit: 3a89b2de5059eee6256b5b4a614a83496acc5709
- Final commit: unchanged

## Pre-existing changes

Preserved all unrelated user changes in the checkout, including Melee gameplay/configuration/assets and prior dev-loop implementation work. This turn changed only `devscripts/dev-loop.py`, `docs/specs/dev-loop/dev-loop.md`, and this changelog.

## Changes with exact before and after text

### `devscripts/dev-loop.py`

- Line 71. Before: `DASHBOARD_FRAME_SECONDS = 1.0`. After: `DASHBOARD_FRAME_SECONDS = 0.05` and a separate `DASHBOARD_STATUS_REFRESH_SECONDS = 1.0` at line 72. This polls keys every 50 ms without forcing the status/rainbow render to run 20 times per second.
- Lines 1620–1623, `start_build_worker`. Before, after the `if self.pending_build_request_ids` / `else` branch, the code used `self.active_build_request_ids = request_ids` without defining `request_ids` when an automatic build had no explicit request. After, it initializes `request_ids: list[str] = []` before the branch. This fixes the traceback that caused the watchdog to repeatedly restart a daemon before startup completed.
- Lines 3216–3240, `ensure_running`. Before: it waited the full daemon-ready timeout before checking whether a daemon should be started. After: it gives the watchdog a 3-second startup opportunity, then uses the daemon lock as the singleton gate for a direct fallback start and waits for authenticated readiness. Watchdog/daemon locks prevent duplicate owners if their starts race.
- Lines 3530–3544 and 3550–3558, dashboard controls/loop. Before, a recognized key was not rendered until after the control request, and the dashboard loop slept for one second between key polls. After, the dashboard immediately renders “sending” feedback, polls every 50 ms, handles a control response's state, and refreshes status/rainbow on a separate one-second cadence.
- Line 3547, `snapshot()`. Status calls now use a 250 ms timeout so a missing daemon does not stall keyboard polling for multiple seconds.
- Lines 3290 onward, `_recent_activity`. Before: `handle.readlines()[-300:]` read the entire journal on every redraw. After: it reads at most the last 64 KiB before selecting recent lines, keeping immediate key feedback bounded even after long sessions.

### `docs/specs/dev-loop/dev-loop.md`

- Lines 2039–2055, new “One launch and automatic availability check”: there was no subsection making the startup order explicit in the canonical controls section. The addition says the user launches only the dev-loop entry point, which checks/starts the watchdog and daemon, waits for authenticated readiness, reuses healthy owners, and leaves game launch explicit on `1`.
- Lines 2057–2068, new “Immediate key feedback”: there was no separate keyboard/render timing contract. The addition specifies 50 ms key polling, immediate sending/error feedback, and one-second normal status/rainbow redraws with immediate redraw on a keypress.

## Findings and reasoning

The user's displayed “daemon unavailable” view corresponded to a real startup failure. `.dev/dev-loop-watch.log` showed `UnboundLocalError` in `start_build_worker()` because a normal automatic build had no correlated CLI request IDs. The watchdog had restarted this crashing daemon 510 times before the fix. Initializing an empty request list restores the normal startup path.

Startup already used the `ensure_running()` entry point, but it waited the full readiness timeout before its daemon fallback. The change makes the watchdog the first start path and uses the daemon's lock to make a short-timeout fallback safe. Dashboard input previously shared the one-second frame sleep; it now has a 50 ms polling interval independent of animation/status cadence.

## Documents and focused review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/documentation-checker-v1.md`.
- Specification/behavior review: PASS for the focused startup and input behavior; the broader existing dev-loop spec still contains an informal TODO at `docs/specs/dev-loop/dev-loop.md:1`: `2026 10 17 1236 jorj todo make htis doc better`. Suggested replacement: outline the document's intended sections and link each to its authoritative owner; this cleanup was outside the requested behavior change.
- Documentation review: PASS for the new text's placement in the canonical dashboard-controls section and for matching the implemented timing and startup behavior.
- No regression record was added because this turn fixed a reproducible startup defect but did not validate the full gameplay regression-record workflow; the Python daemon startup and dashboard path are separate from game runtime behavior.

## Validation and remaining human review

- `python -m py_compile devscripts/dev-loop.py`: PASS.
- `git diff --check -- devscripts/dev-loop.py docs/specs/dev-loop/dev-loop.md`: PASS.
- The 64 KiB journal-tail change was syntax-checked; dashboard recent activity remains capped at 300 parsed lines and three displayed events.
- `python devscripts/dev-loop.py --ensure-running`: PASS. It reported the watchdog and daemon running, build 2580 published/current, and no game run. The initial pipeline reported `Nothing changed.`
- Ran the visible dashboard for two scripted harmless keys (`2`, then `Esc`) using `--dashboard-frames 2`. The output showed `launch-mode picker open` and `launch-mode picker cancelled`; it did not start a client/server or change the selected mode. The dashboard showed the existing watchdog PID 3716 and daemon PID 25596.
- A subsequent read-only status check reported the same daemon PID, no client/server, and build 2580 current. Watcher state and daemon state reported the same script hash. The watcher restart count was 517 after recovery; the last log tail showed the initial build succeeded and no further traceback.
- After the bounded-journal refinement changed the watched script, the idle watcher performed one clean replacement. Final read-only evidence reported daemon PID 12612, matching on-disk/daemon script hash `f78fda05d8726c691002f903dc91555862e42b21256a045f7ee920024deb36f9`, no active run, and build 2581 published/current (`Nothing changed.`).
- No C++ build, game launch, `[1]` runtime action, or human visual playtest was performed. Startup/infrastructure readiness and harmless dashboard-key handling were verified; game launch remains explicit and was intentionally not triggered.
