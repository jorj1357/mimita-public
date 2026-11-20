# Dev Loop, Logging, and Build regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 30
---

## Evidence-backed recurring problems and fixes

| Recurring pattern | Evidence-backed response |
|---|---|
| A stale or unrelated process/build is mistaken for the live run. | Correlate run, build ID, PID, instance journal, and manifest through one dev loop. |
| Build/reload boundaries or journal ownership hide source generation. | Record build generation and runtime identity. Cold-build debt is tracked here. |

---

## Entry 1 — Cold Build Required

- Recorded: 2026-09-20T15:11:13Z
- Status: COLD-BUILD DEBT
- Summary: This persistent record tracks intentional cold builds required because a changed owner cannot yet cross the live-reload boundary. A cold build is not automatically proof of a user-visible behavior regression.
- Detail: [cold-build-required-REG-2026-09-20.md](cold-build-required-REG-2026-09-20.md)

## Entry 2 — Cold Build Required

- Recorded: 2026-09-20T15:11:13Z
- Status: COLD-BUILD DEBT
- Summary: Historical root template reference only; the active cold-build debt record is Entry 1.
- Detail: [cold-build-required-REG.md](../cold-build-required-REG.md)

## Entry 3 — Agent File Visibility Mismatch

- Recorded: 2026-09-21T01:23:53Z
- Status: UNRESOLVED
- Summary: docs/architecture/time-and-formatting/time-and-formatting.md
- Detail: [agent-file-visibility-REG-2026-09-21.md](agent-file-visibility-REG-2026-09-21.md)

## Entry 4 — Development Loop Bypassed the GUI Room-Code/ICE Join Path

- Recorded: 2026-09-27T01:16:26Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/networking/networking.md
- Detail: [dev-loop-bypassed-REG-2026-09-27.md](dev-loop-bypassed-REG-2026-09-27.md)

## Entry 5 — Development Loop Status Menu Redrew Forever While Idle

- Recorded: 2026-09-27T01:16:26Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/performance/performance.md
- Detail: [dev-status-menu-REG-2026-09-27.md](dev-status-menu-REG-2026-09-27.md)

## Entry 6 — Runtime DLL staging lock was reported as a failed build

- Recorded: 2026-09-27T01:31:34Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/operations/build-and-exe/build-and-exe.md
- Detail: [runtime-dll-staging-REG-2026-09-27.md](runtime-dll-staging-REG-2026-09-27.md)

## Entry 7 — Dev loop looked frozen after the server crashed before room-code startup

- Recorded: 2026-09-27T01:45:00Z
- Status: ATTEMPTED FIX (2)
- Summary: docs/operations/task-completion/task-completion.md
- Detail: [server-crash-frozen-REG-2026-09-27.md](server-crash-frozen-REG-2026-09-27.md)

## Entry 8 — Automatic dev client was not recognized as the server host

- Recorded: 2026-09-27T01:58:28Z
- Status: SOLUTION AS OF 2026-09-27T02:05:22Z
- Summary: docs/specs/networking/networking.md
- Detail: [dev-loop-first-REG-2026-09-27.md](dev-loop-first-REG-2026-09-27.md)

## Entry 9 — Dev loop stopped at zero processes after a server startup crash

- Recorded: 2026-09-27T15:48:01Z
- Status: FIXED (configuration startup blocker); dev-loop fallback behavior remains
- Summary: covered by the orchestration fix below.
- Detail: [server-crash-retry-REG-2026-09-27.md](server-crash-retry-REG-2026-09-27.md)

## Entry 10 — Dev loop relaunched a stale published build after compilation failed

- Recorded: 2026-09-27T19:54:00Z
- Status: FIXED (source-level; live dev-loop restart still required)
- Summary: devscripts/dev-loop.py
- Detail: [dev-loop-stale-REG-2026-09-27.md](dev-loop-stale-REG-2026-09-27.md)

## Entry 11 — Dev-loop server closed when the loop/client lifetime changed

- Recorded: 2026-09-29T13:56:27Z
- Status: ATTEMPTED FIX (2)
- Summary: docs/specs/gamemodes/gamemodes.md
- Detail: [dev-loop-server-REG-2026-09-29.md](dev-loop-server-REG-2026-09-29.md)

## Entry 12 — Dev-loop Published EXE Crash

- Recorded: 2026-10-04T20:19:34Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/operations/build-and-exe/build-and-exe.md
- Detail: [dev-loop-zero-REG-2026-10-04.md](dev-loop-zero-REG-2026-10-04.md)

## Entry 13 — Ragdoll JSONL Process Ownership Regression

- Recorded: 2026-10-06
- Status: OPEN
- Summary: During a live Counter-Strike session, the user opened the path reported by the game and found that the first record said process=server. The journal did not contain the expected death.ragdoll.requested or ragdoll.corpse. records, so it was unclear whether the client was logging, whether the wrong
- Detail: [ragdoll-jsonl-process-REG-2026-10-06.md](ragdoll-jsonl-process-REG-2026-10-06.md)

## Entry 14 — dev-loop helper PowerShell window flood

- Recorded: 2026-10-17T15:08:51Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/dev-loop/dev-loop.md sections 5.1, 10.1, and 19.4.
- Detail: [dev-loop-helper-REG-2026-10-17.md](dev-loop-helper-REG-2026-10-17.md)

## Entry 15 — Dev-loop status write race and startup timeout

- Recorded: 2026-10-21T23:20:30Z
- Status: ATTEMPTED FIX (1)
- Summary: The dev-loop entry point must reuse or start one watchdog and daemon, wait for the authenticated daemon status endpoint, and remain usable when invoked soon after another launch. The current specification says: “It starts whichever
- Detail: [dev-loop-status-REG-2026-10-21.md](dev-loop-status-REG-2026-10-21.md)

## Entry 16 — Cached client-ready flags suppressed a new launch

- Recorded: 2026-10-22T21:49:48Z
- Status: ATTEMPTED FIX (1)
- Summary: - Expected behavior: a Juggernaut launch request starts a client when no client process is alive. - Actual behavior: the loop reported a successful/reusable launch while clientpids was empty; the run ID and journal did not advance.
- Detail: [dev-loop-stale-REG-2026-10-22.md](dev-loop-stale-REG-2026-10-22.md)

## Entry 17 — Dev-loop UTC artifact folder launch mismatch

- Recorded: unknown
- Status: FIX IMPLEMENTED — dashboard launch and live server/client identity verified; rendered gameplay still needs human visual review.
- Summary: Status: FIX IMPLEMENTED — dashboard launch and live server/client identity verified; rendered gameplay still needs human visual review.
- Detail: [dev-loop-utc-REG-2026-11-03.md](dev-loop-utc-REG-2026-11-03.md)

## Entry 18 — Canonical scenario capture had two sinks

- Recorded: 2026-11-03T19:09:00Z
- Status: ATTEMPTED FIX (1) — journal-only source path; runtime review pending
- Summary: The client wrote the per-tick scenario file and the journal, while the server wrote a separate canonical journal copy; each executable now has one events.jsonl sink.
- Detail: [scenario-journal-only-REG-2026-11-03.md](scenario-journal-only-REG-2026-11-03.md)

Correction 2026-11-03: the journal-only sink was reversed. The full recording is
saved under `devscripts/scenarios/user/`, the server sidecar is merged at stop,
and the journal keeps only compact start/stop/invalid summaries. See the
correction in the detail file.

## Entry 19 — Recording stop blocked the client into a reconnect

- Recorded: 2026-11-04T01:00:00Z
- Status: ATTEMPTED FIX (1) — async merge and streaming source; long human session pending
- Summary: `scenario_endrecord` busy-waited six seconds in the client main loop and merged synchronously, starving network heartbeats. The command now returns immediately and merge runs in the background with a streaming two-pass read.
- Detail: [recording-stop-client-REG-2026-11-04.md](recording-stop-client-REG-2026-11-04.md)

## Entry 20 — Incomplete newest artifact prevented daemon startup

- Recorded: 2026-11-06T01:33:29Z (2026-11-05 20:33:29 EST)
- Status: FIXED — daemon recovery and latest-artifact launch verified; visual review pending
- Summary: A failed executable publication left a timestamped folder without its identity sidecar. Startup selected the newest folder without validating it and crashed while reading the missing sidecar, so the dashboard could not deliver its launch request.
- Detail: [latest-artifact-pointer-REG-2026-11-05.md](latest-artifact-pointer-REG-2026-11-05.md)

## Entry 21 — Stable launch swap raced process exit

- Recorded: 2026-11-06T02:06:13Z (2026-11-05 21:06:13 EST)
- Status: ATTEMPTED FIX — bounded retry is in source; live daemon restart is deferred while the healthy game run is active
- Summary: Windows denied renaming `.dev/current` immediately after the prior MiMITA processes were terminated, so a latest-artifact launch failed during stable-path activation.
- Detail: [stable-launch-swap-lock-REG-2026-11-05.md](stable-launch-swap-lock-REG-2026-11-05.md)

## Entry 22 — Build superseded during concurrent edits

- Recorded: 2026-11-06T04:04:40Z (2026-11-05 23:04:40 EST)
- Status: OPEN — observability and workflow risk; final build succeeded
- Summary: A two-agent session changed 68 paths, including six widely included headers. The dev-loop's one-build-at-a-time policy queued rebuilds, source generation advanced 28→92, and the `03:08:24Z` artifact became stale before launch. The final `03:56:32Z` build succeeded in 221.8 seconds and was launched. A later failure was caused by only ~113 MB free disk space; freeing generated build cache/artifacts allowed the canonical build to publish `04:33:32Z`. This was not a lost build. Terminal windows seen during the session were Git scans from OpenCode/ChatGPT, not MiMITA build activity.
- Detail: [build-superseded-stale-REG-2026-11-06.md](build-superseded-stale-REG-2026-11-06.md)

## Entry 23 — Git index became unreadable after an interrupted work session

- Recorded: 2026-11-07
- Status: OPEN — abrupt PC shutdown during AI work is suspected by the user; cause is unconfirmed.
- Summary: Git reported `bad signature 0x00000000` for `.git/index`, blocking status, staging, and commit operations. The index was preserved and rebuilt from `HEAD`; the user's suspected shutdown was not independently verified.
- Detail: [git-index-corrupt-interrupted-session-REG-2026-11-07.md](git-index-corrupt-interrupted-session-REG-2026-11-07.md)

## Entry 24 — Visible dev-loop launch left the game behind other windows

- Recorded: 2026-11-08
- Status: PARTIALLY FIXED — launch visibility/restore behavior changed; foreground activation and taskbar presence remain unverified.
- Summary: The dev loop showed the client window without activating it, then restored the previous foreground app. A server-reuse path also left `prelaunch_windows` unset and crashed during launch completion.
- Detail: [visible-game-window-REG-2026-11-08.md](visible-game-window-REG-2026-11-08.md)

## Entry 25 — Coordinator dashboard stayed on connecting status

- Recorded: 2026-11-09T20:50:40Z (2026-11-09 15:50:40 EST)
- Status: FIXED — dashboard status and lane list refreshed live after adding the missing import.
- Summary: The coordinator's lane-status collector called `re.fullmatch` without importing `re`. Its refresh handler displayed the exception while the UI remained on “Connecting to the build/runtime service…”.
- Detail: [dashboard-status-refresh-REG-2026-11-09.md](dashboard-status-refresh-REG-2026-11-09.md)

## Entry 26 — Incremental game-DLL builds recompiled every source

- Recorded: 2026-11-12
- Status: FIXED (source/build) — no-change builds now skip all 575 sources; runtime proof is not applicable to this build-system fix.
- Summary: The whole-game DLL worker treated `-MP` phony targets and Windows drive letters in `.d` files as missing dependencies, and its object cache was scoped to one generation. Every build recompiled all 575 sources. The parser now matches `build.py::source_changed`, the cache is persistent, and a no-change run prints 575 `[SKIP]` lines with zero compiles.
- Detail: [incremental-recompile-REG-2026-11-12.md](incremental-recompile-REG-2026-11-12.md)

## Entry 27 — Dev loop targeted an unowned GUI server during process cleanup

- Recorded: 2026-11-17T21:23:13Z
- Status: OPEN — cleanup target ownership is not verified.
- Summary: The process-cap cleanup scans every same-checkout `mimita.exe` and calls `taskkill` without proving the process belongs to the active dev-loop run. The captured GUI server target returned `taskkill_rc=128`, so termination is unverified; dev-loop-owned server/client targets returned `0`.
- Detail: [unowned-process-reaper-REG-2026-11-17.md](unowned-process-reaper-REG-2026-11-17.md)

## Entry 28 — GUI Juggernaut launch inherited Captain Falcon weapons

- Recorded: 2026-11-17T21:52:47Z
- Status: FIXED — GUI launch mode 9 and Juggernaut NPC role loadouts verified in the live journal; separate in-game mode-switch command remains unverified.
- Summary: A stale one-shot `--weapon-set 12` survived mode selection and forced Captain Falcon loadouts. Mode changes now clear it, and Auto GUI launches ignore stale weapon values. A fresh mode 9 launch omitted the override, resolved the gamemode default, and equipped Juggernaut NPCs with their configured Heavy Shotgun, Large Machine Gun, and Force Punch role loadout.
- Detail: [juggernaut-gui-weapon-set-REG-2026-11-17.md](juggernaut-gui-weapon-set-REG-2026-11-17.md)

## Entry 29 — DLL build time accounting hid long cache scans

- Recorded: 2026-11-19
- Status: FIXED for the observed display, scan-reporting, and split-clock failures — a live generation 353 warm no-op measured 0.403 s worker time; generation 348's generic exit code 2 cause remains unknown.
- Summary: The dashboard showed one TU's 46 ms dependency check beside a 642-TU scan and printed initialized zero compile/link/total fields. A generation 348 GUI failure at ~364 s disagreed with the run-journal duration (~470 s), with no retained failed-source cause. The worker also performs per-TU synchronous progress reporting during serial dependency scanning.
- Detail: [dll-build-time-accounting-REG-2026-11-19.md](dll-build-time-accounting-REG-2026-11-19.md)
- Later evidence (2026-11-19): the published generation 351 attempt measured 1658.10 s of synchronous per-skip progress/record overhead inside a 1716.38 s cache scan; detail is in the linked record.
- Lean-scan evidence (2026-11-19): an intermediate generation 352 publish measured a 14.22 s scan (3.95 s parse, 9.03 s stat, 1,496 unique dependencies) versus 315-1716 s earlier.
- Verified (2026-11-19): a live generation 353 warm no-op completed in 0.403 s worker time with a 0.096 s marker scan, 642 skips, no compile/link/publish, and a 36.346 s complete request-to-result interval. The batched scan and worker clock are live-verified.
- Repeated (2026-11-19): a second warm no-op measured 0.290 s worker time, 0.052 s scan, 642 skips, and 9.927 s complete request-to-result. Worker-boundary latency is verified and repeatable; codegen/manifest snapshot remain the dominant complete-interval stages.

## Entry 30 — Dashboard maximize and restart snapped to the primary monitor

- Recorded: 2026-11-20
- Status: FIXED — three live close/reopen cycles and maximize/restore retained the left secondary monitor; saved coordinates and keepalive window rectangles matched.
- Summary: `devscripts/dev_loop/gui.py`, `devscripts/dashboard-keepalive.py`
- Detail: [dashboard-monitor-snap-REG-2026-11-20.md](dashboard-monitor-snap-REG-2026-11-20.md)
