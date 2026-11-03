# Dev Loop, Logging, and Build regressions

Append-only master ledger. Each entry links to the detailed dated record that owns the full evidence.

Entries: 17

---

## Entry 1 — Cold Build Required

- Recorded: 2026-09-20T15:11:13Z
- Status: COLD-BUILD DEBT
- Summary: This persistent record tracks intentional cold builds required because a changed owner cannot yet cross the live-reload boundary. A cold build is not automatically proof of a user-visible behavior regression.
- Detail: `docs/regressions/2026-09-20/cold-build-required-REG.md`

## Entry 2 — Cold Build Required

- Recorded: 2026-09-20T15:11:13Z
- Status: COLD-BUILD DEBT
- Summary: This persistent record tracks every intentional cold build and the work needed to move that boundary into the live-reload path. A cold build is not by itself a user-visible behavior regression.
- Detail: `docs/regressions/cold-build-required-REG.md`

## Entry 3 — Agent File Visibility Mismatch

- Recorded: 2026-09-21T01:23:53Z
- Status: UNRESOLVED
- Summary: docs/architecture/time-and-formatting/time-and-formatting.md
- Detail: `docs/regressions/2026-09-21/agent-file-visibility-mismatch-REG.md`

## Entry 4 — Development Loop Bypassed the GUI Room-Code/ICE Join Path

- Recorded: 2026-09-27T01:16:26Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/networking/networking.md
- Detail: `docs/regressions/2026-09-27/dev-loop-bypassed-room-code-REG.md`

## Entry 5 — Development Loop Status Menu Redrew Forever While Idle

- Recorded: 2026-09-27T01:16:26Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/performance/performance.md
- Detail: `docs/regressions/2026-09-27/dev-status-menu-redraw-loop-REG.md`

## Entry 6 — Runtime DLL staging lock was reported as a failed build

- Recorded: 2026-09-27T01:31:34Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/operations/build-and-exe/build-and-exe.md
- Detail: `docs/regressions/2026-09-27/runtime-dll-staging-lock-REG.md`

## Entry 7 — Dev loop looked frozen after the server crashed before room-code startup

- Recorded: 2026-09-27T01:45:00Z
- Status: ATTEMPTED FIX (2)
- Summary: docs/operations/task-completion/task-completion.md
- Detail: `docs/regressions/2026-09-27/dev-loop-server-crash-looks-frozen-REG.md`

## Entry 8 — Automatic dev client was not recognized as the server host

- Recorded: 2026-09-27T01:58:28Z
- Status: SOLUTION AS OF 2026-09-27T02:05:22Z
- Summary: docs/specs/networking/networking.md
- Detail: `docs/regressions/2026-09-27/dev-loop-first-client-not-host-REG.md`

## Entry 9 — Dev loop stopped at zero processes after a server startup crash

- Recorded: 2026-09-27T15:48:01Z
- Status: FIXED (configuration startup blocker); dev-loop fallback behavior remains
- Summary: covered by the orchestration fix below.
- Detail: `docs/regressions/2026-09-27/dev-loop-server-crash-retry-and-rainbow-REG.md`

## Entry 10 — Dev loop relaunched a stale published build after compilation failed

- Recorded: 2026-09-27T19:54:00Z
- Status: FIXED (source-level; live dev-loop restart still required)
- Summary: devscripts/dev-loop.py
- Detail: `docs/regressions/2026-09-27/dev-loop-stale-build-launch-REG.md`

## Entry 11 — Dev-loop server closed when the loop/client lifetime changed

- Recorded: 2026-09-29T13:56:27Z
- Status: ATTEMPTED FIX (2)
- Summary: docs/specs/gamemodes/gamemodes.md
- Detail: `docs/regressions/2026-09-29/dev-loop-server-closes-REG.md`

## Entry 12 — Dev-loop Published EXE Crash

- Recorded: 2026-10-04T20:19:34Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/operations/build-and-exe/build-and-exe.md
- Detail: `docs/regressions/2026-10-04/dev-loop-zero-filled-exe-REG.md`

## Entry 13 — Ragdoll JSONL Process Ownership Regression

- Recorded: 2026-10-06
- Status: OPEN
- Summary: During a live Counter-Strike session, the user opened the path reported by the game and found that the first record said process=server. The journal did not contain the expected death.ragdoll.requested or ragdoll.corpse. records, so it was unclear whether the client was logging, whether the wrong
- Detail: `docs/regressions/2026-10-06/ragdoll-jsonl-process-ownership-REG.md`

## Entry 14 — dev-loop helper PowerShell window flood

- Recorded: 2026-10-17T15:08:51Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/dev-loop/dev-loop.md sections 5.1, 10.1, and 19.4.
- Detail: `docs/regressions/2026-10-17/dev-loop-helper-powershell-window-flood-REG.md`

## Entry 15 — Dev-loop status write race and startup timeout

- Recorded: 2026-10-21T23:20:30Z
- Status: ATTEMPTED FIX (1)
- Summary: The dev-loop entry point must reuse or start one watchdog and daemon, wait for the authenticated daemon status endpoint, and remain usable when invoked soon after another launch. The current specification says: “It starts whichever
- Detail: `docs/regressions/2026-10-21/dev-loop-status-write-race-REG.md`

## Entry 16 — Cached client-ready flags suppressed a new launch

- Recorded: 2026-10-22T21:49:48Z
- Status: ATTEMPTED FIX (1)
- Summary: - Expected behavior: a Juggernaut launch request starts a client when no client process is alive. - Actual behavior: the loop reported a successful/reusable launch while clientpids was empty; the run ID and journal did not advance.
- Detail: `docs/regressions/2026-10-22/dev-loop-stale-ready-suppressed-launch-REG.md`

## Entry 17 — Dev-loop UTC artifact folder launch mismatch

- Recorded: unknown
- Status: FIX IMPLEMENTED — dashboard launch and live server/client identity verified; rendered gameplay still needs human visual review.
- Summary: Status: FIX IMPLEMENTED — dashboard launch and live server/client identity verified; rendered gameplay still needs human visual review.
- Detail: `docs/regressions/2026-11-03/dev-loop-utc-folder-launch-REG.md`

## Entry 18 — Canonical scenario capture had two sinks

- Recorded: 2026-11-03T19:09:00Z
- Status: ATTEMPTED FIX (1) — journal-only source path; runtime review pending
- Summary: The client wrote the per-tick scenario file and the journal, while the server wrote a separate canonical journal copy; each executable now has one events.jsonl sink.
- Detail: `docs/regressions/2026-11-03/scenario-journal-only-REG.md`
