# Independent bounded review of issue-0cdac76fb006

- Task ID: wake-dd6eb961b752
- Agent: issue-review-sweeper
- Focus: one read-only review of the small-desktop proof standard issue, its linked task/proofs, the exemplar dashboard monitor-retention proof, the dashboard state after a real restart, the dev-loop attach journal, the keepalive journal, and keepalive window selection
- Summary: Appended one independent review (review id 491, verdict `inconclusive`) to `issue-0cdac76fb006`, one evidence-backed process suggestion to the same issue, and one issue-linked message (msg-504fd6f1945b) to owner `dashboard-position-proof`. Executed the documented single-shot `dashboard-keepalive.py --once` (no `--focus`) and class-verified the sampled window. No source edit, build request, test run, close, restart, owner change, or fix claim.
- Status: complete (review only)
- Date, time, timezone: 2026-11-26T22:52Z, ISO 8601

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents (including config files, `devscripts/dev_loop/game_build.py`, `docs/issues/README.md`, and many changelogs). This session edited no source, reverted nothing, and touched only the issue record through `agent-coord.py`, one keepalive sample, coordination state, and this changelog. No compiler, `build.py`, module worker, second dev-loop, or runtime test run was invoked.

# Requested behavior

Run one bounded independent review of `issue-0cdac76fb006`: inspect issue brief/readiness, proof status, linked artifacts, and the latest relevant journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and a next discriminating check; then a short development-process review with one specific proposal to the best existing improvement issue (or none if unsupported). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 3, 4: coordination begin/sync/observe; observations separated from inference; no compiler/build/second dev-loop in a review wake.
- `docs/specs/dev-loop/dashboard.md`: the dashboard must persist its normal size/position and restore them on startup; keepalive must leave a user-placed window where it is; monitor coordinates may be negative.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build or a live process is not behavior proof; acceptance requires a pinned run journal with the required event.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` | 5 reviews (ids 46/191/288/372/435) + one suggestion | + review id 491 + one process suggestion + issue-discussion message msg-504fd6f1945b | required independent review record | remove only the appended review/suggestion/message text |
| `.dev/agent-coord/brain.db` | no review id 491 | review id 491, suggestion, message msg-504fd6f1945b | coordination record | restore from backup if withdrawn |
| `.dev/dashboard-keepalive/events.jsonl` | last event 2026-11-25T19:31:01Z | + one `dashboard.restored` event at 2026-11-26T22:44:09.589Z (console window, see Findings) | bounded live check | remove only the last line |
| `docs/changelog/2026-11-26/20261126_1752-issue-0cdac76fb006-independent-review.md` | absent | new changelog | one changelog per repository-touching session | delete file |

# Contract / ABI changes

None.

# State / schema changes

- `issue_review` added review id 491 to `issue-0cdac76fb006` (verdict `inconclusive`; state stays `in_review`, owner unchanged).
- One `issue suggest` record appended to `issue-0cdac76fb006`.
- One issue-linked `message` msg-504fd6f1945b sent to owner `dashboard-position-proof` (thread thread-8895bece6998).

# Diagnostics

- Readiness: `ready=false`, score 1/8, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk; the only linked task `small-desktop-state-evidence-standard` is still `open`.
- Proof `task-proof-bac2b095c7af` still `unproven`, `verified_utc` empty, evidence empty. Pinned doc hashes still stale and unchanged since 11-24: `issue-intake-and-triage.md` 56d48ea9... vs current f3d370b5...; `agent-proof-and-verification.md` 9d2eaefb... vs current eaef36e8....
- Proof `task-proof-ed45c1b9e923` still `stale` ("source changed: devscripts/dev_loop/gui.py"); current gui.py sha256 5ad5428f... (11-25 sweep: 267e69ea...). The two gui.py commits since then (f9d94c5b at 2026-11-25T21:27Z; 0794fe4e at 2026-11-26T05:24Z) do not touch `_window_bounds` (gui.py:89), `_tk_geometry` (:112), or `persist_window_geometry` (:919). `close_view` (gui.py:1154-1177) still calls `persist_window_geometry()` then `root.destroy()` and still differs from proven commit d3368058. `dashboard-keepalive.py` still matches recorded d3cc3e94....
- New live fact - real restart with secondary position restored: new dashboard PID 35600 (`python.exe devscripts/dev-loop.py --dashboard --no-terminal-relaunch`) started 2026-11-26T22:43:09Z; `dev-loop.dashboard.attach` logged dashboard_pid=35600 at 2026-11-26T22:43:10.940Z (run 20261126_201711, watchdog 30360, daemon 10336). Pre-restart `.dev/dashboard-ui.json` was `775x810+-1769+129`; the new process wrote the same geometry at 22:43:22Z and 22:47:10Z. Live Tk window hwnd 0x1c00982 (class TkTopLevel) is visible, not minimized, rect [-1769,129,-994,939] = 775x810 at (-1769,129), exactly the saved secondary geometry. One PID, no duplicate Tk window.
- New tooling fact - keepalive sampled the wrong window: `dashboard-keepalive.py --once` at 2026-11-26T22:44:09.589Z selected hwnd 36640296 (0x22F1628), class ConsoleWindowClass, rect [120,77,1015,595], visible=false, not the Tk dashboard. `find_window` matches title substring only (dashboard-keepalive.py:109-125) and the dev-loop console shares the title (dev-loop.py:7578,7594), so it emitted `dashboard.restored` and called ShowWindow on the console. The keepalive journal now has 6,042 lines; previous sample was 2026-11-25T19:31:01Z; no continuous keepalive process exists.
- Deliverable (the reusable small-desktop/persistent-desktop proof standard) is still absent from docs; issue state unchanged.

# Validation

- `agent-coord.py begin/sync/observe`, `issue brief`, `issue readiness`, `proof status`, `proof list`, `context list`, source/doc SHA-256 via Python, direct Win32 window enumeration with class names, journal reads, and `git status`.
- `dashboard-keepalive.py --once` (no `--focus`) executed; one event emitted and then class-verified against the live Tk window.
- `issue review` appended review id 491; `issue suggest` appended one proposal; `send --issue` posted one owner message.
- No compiler, build request, module worker, second dev-loop, or runtime test run was invoked. No dashboard close/restart performed.

# Findings

1. The standard deliverable is still unimplemented; readiness remains 1/8 and the only task is open with an unproven proof whose pinned doc hashes are stale.
2. A real dashboard restart at 2026-11-26T22:43:09Z restored the saved secondary position (x=-1769, 775x810) and the window stayed there; this is the first post-restart retention observation in six sweeps, but only one cycle and without a bracketing keepalive sample of the transition.
3. The documented `--once` check is not a reliable dashboard-window sample: it can select the dev-loop console window that carries the same title, producing a false primary-monitor rect and a false `dashboard.restored` event (observed this sweep). Retention evidence must verify hwnd class TkTopLevel and rect.
4. The exemplar proof's whole-file staleness persists, but the geometry measurement/save regions are still unchanged; the close_view difference found on 11-23 remains unresolved and untested.

# Confidence

High on readiness/proof-hash facts, the restart timestamps/PIDs, the live/saved geometry match on the secondary, the console-window sample, and journal timestamps (direct reads). Moderate on attributing the restart cause (explicit relaunch vs daemon/watchdog replacement) - the attach event proves a new dashboard PID, not the cause. Unknown: whether retention survives three close/reopen cycles, and whether the changed close_view path affects save-before-exit.

# Proof

No proof claim. Read-only review; evidence is source-tier, runtime-state-tier, and journal-tier. No behavior change introduced.

# Regression review

- Regression entry appended: no
- Why: no confirmed code change or fix; observations recorded on the issue.

# Human acceptance

- Visual, gameplay, multiplayer review: none (not applicable)
- Still unverified: three close/reopen cycles captured with class-filtered TkTopLevel rects, `.dev/dashboard-ui.json` before/after, one dashboard PID per cycle, and geometry written before exit.

# Agent handoff context

- Owner: `dashboard-position-proof` (messaged, msg-504fd6f1945b).
- Next exact step: run `python devscripts/dashboard-keepalive.py --interval 5` (no `--focus`) for 10 minutes, do 3 close/reopen cycles from the current secondary position (x=-1769), and per cycle record class-filtered TkTopLevel hwnd+rect, `.dev/dashboard-ui.json`, one dashboard PID with the old PID exited, and the `dev-loop.dashboard.attach` line. User context ctx-2d62389bbf7f authorizes dashboard restart/run for measurement. Fix or guard keepalive `find_window` to require class TkTopLevel before using its output as evidence. Then re-pin `task-proof-bac2b095c7af` to f3d370b5.../eaef36e8... and re-verify `task-proof-ed45c1b9e923` by the named geometry+close_view region.
- What another agent should not redo: do not treat a title-matched keepalive sample as dashboard evidence, do not infer a snap from saved==live, and do not claim retention from one restart cycle or the pre-existing 11-20 proof alone.

# Automatic evidence

- Git status: shared dirty tree preserved; this session added one issue review, one issue suggestion, one owner message, one keepalive event, coordination DB changes, and this changelog
- Build identity: latest published game-DLL generation 730 (`src/gamemode/arena-ball.cpp`, `src/vehicles/vehicle-system.cpp`); unrelated to this issue; no build requested
- Journal paths checked: `.dev/dashboard-keepalive/events.jsonl`, `.dev/dashboard-ui.json`, `logs/2026-11-26/20261126_201711/dev-loop/events.jsonl`; live windows via Win32 on PID 35600

# How to reverse

- Remove only the appended review (id 491), suggestion, and message text from the `issue-0cdac76fb006` doc; do not rewrite prior text.
- Remove only the appended 2026-11-26T22:44:09.589Z `dashboard.restored` line if the sample is withdrawn.
- Restore `.dev/agent-coord/brain.db` from backup if the message/review is withdrawn.
- Delete this changelog. No commit to revert.
