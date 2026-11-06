# Dev Loop, Logging, and Build regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 21
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
