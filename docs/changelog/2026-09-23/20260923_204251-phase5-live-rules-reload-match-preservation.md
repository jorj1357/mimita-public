# Phase 5: live rules change does not reset the match (replication/gamemode policy)

Date (UTC): 2026-09-23T20:42:51Z
Status: implemented; build and selftests verified; cold paths intentionally retained

## Scope

Phase 5 of the hot server/networking migration: confirm the replication and
gamemode policy owners are hot, and prove the acceptance criterion "a live rules
change does not reset the match". Per instruction, no old/cold paths were
deleted.

## Findings (audit, no code change needed)

- **Snapshot relevance/priority/frequency**: `net.relevance` already owns
  include/tier/low-tier cadence per viewer; the kernel gathers candidates and
  applies the decision mechanically. Proven by `--relevance-policy-selftest`.
- **Send decision**: `net.send-policy` owns whether a viewer receives a snapshot
  this tick.
- **Reliable delivery**: `net.reliable-policy` owns retry/keep-connection.
- **Gamemode policy**: FFA/TDM/counterstrike modes own phase, scoring, win,
  lifecycle, assignment, and respawn; the cold phase machine is bypassed via
  `MatchPhaseOwnership`. Verified by `--match-policy-selftest`,
  `--gamemode-hot-selftest`, `--counterstrike-selftest`.

## Change

- **New self-test** (`--live-rules-reload-selftest`): runs a hot TDM match to an
  active state with a live score, then performs a real generation swap
  (`HotReloadSystem::unloadGameDLL()` + `startup()`), and asserts:
  - the package re-activates and the reloaded generation re-registers the mode;
  - `MatchPhaseOwnership` survives;
  - phase, red/blue scores, round, and participants are unchanged;
  - the reloaded rules continue the match (no reset to waiting).
  This is the concrete proof of the "THE LAWS CHANGED / THE UNIVERSE DID NOT
  RESTART" invariant for gamemode/match policy.

This also serves as the answers to the client-side question: the same in-process
DLL edit/reload mechanism that preserves a running match is what lets a listen
server change its behavior live for the local player.

## Evidence

- Source changes: `live-rules-reload-selftest.{h,cpp}`, `game-cli.cpp`.
- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`
  (`mimita-20260923T164145.exe`). A running `mimita.exe` was not touched.
- Automated tests (test evidence):
  - `--live-rules-reload-selftest` -> PASS (10 checks).
  - `--match-policy-selftest`, `--gamemode-hot-selftest`,
    `--counterstrike-selftest`, `--relevance-policy-selftest`,
    `--live-code-selftest`, `--server-journal-selftest`,
    `--actor-lifecycle-selftest`, `--lagcomp-history-selftest`,
    `--packet-codec-selftest` -> PASS.
- Runtime evidence: none beyond the in-process generation swap in the selftest.
- Human acceptance: pending.

## Not done (and deliberately not deleted)

- Cold replication framing (`snapshot-chunks.cpp`), transport, and the
  unmigrated cold gamemode paths are retained. Phase 6 (deletion) was not
  performed by instruction.
- No live two-process run.
