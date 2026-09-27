# Phase 2b: unified lag-compensation policy + live-edit proof

Date (UTC): 2026-09-23T20:09:21Z
Status: implemented; build and selftests verified; live-edit falsification performed

## Scope

Phase 2b of the hot server/networking migration: collapse the two lag-compensation
stages (rewind target tick + pose selection) into one shared hot policy body so a
single source edit changes the whole ordering live, and prove it by falsification.

## Changes

1. **One shared policy body** (`src/hot-reload/hot-lagcomp-policy.h`, new, in the
   manifest headers):
   - `MimitaLagComp::selectTargetTick(...)` — stage 1, the previous `net.rewind`
     body (command/latency/interp compensation, max-rewind clamp,
     generation-mismatch reject);
   - `MimitaLagComp::selectPose(...)` — stage 2, the previous `history.select`
     body (exact/interpolated/nearest/generation clamp);
   - both inline in one file, one `kSimHz`.

2. **Thin seam handlers**: `modules/rewind-policy.cpp` and
   `modules/history-select-policy.cpp` now just call the shared functions. The
   EXE seams (`net.rewind`, `history.select`) are unchanged; only the hot policy
   body is unified.

3. **End-to-end selftest** (`--lagcomp-history-selftest`): drives stage 1, feeds
   the chosen tick into stage 2 over the same history, and asserts the composed
   pose. Now 10 checks.

## Live-edit falsification (performed)

- Edited only `hot-lagcomp-policy.h` (`interpTicks = 0`), rebuilt the hot DLL,
  ran the **unchanged** EXE:
  - `--lagcomp-history-selftest` -> FAIL ("unified: stage 1 selects the rewind
    target tick");
  - `--rewind-policy-selftest` -> FAIL ("interpolation-delay compensation rewinds
    by interp ticks").
- Restored the header, rebuilt the DLL -> both PASS again.

This is the required proof that a single edit to one hot source file changes both
lag-compensation stages live, with no EXE rebuild.

## Evidence

- Source changes: `hot-lagcomp-policy.h`, `modules/rewind-policy.cpp`,
  `modules/history-select-policy.cpp`, `lagcomp-history-selftest.cpp`,
  `hot-modules.json`.
- Build (source/build evidence):
  - Hot DLL: `python build_game_dll.py` -> `DLL build success`.
  - Cold EXE: `python build_agent.py` -> `Status: SUCCESS`
    (`mimita-20260923T160857.exe`). A running `mimita.exe` was not touched.
- Automated tests (test evidence):
  - `--lagcomp-history-selftest` -> PASS (10 checks, incl. the unified chain).
  - `--rewind-policy-selftest` -> PASS.
  - `--live-code-selftest`, `--packet-codec-selftest`,
    `--transport-generation-selftest`, `--capability-selftest`,
    `--reconciliation-policy-selftest`, `--interpolation-policy-selftest`,
    `--hitscan-outcome-selftest` -> PASS.
- Live/runtime evidence: the falsification was executed against the unchanged EXE
  (DLL-only edit), which is the live-edit mechanism. No live two-process session.
- Human acceptance: pending.

## Not done

- Live two-process lag-compensation trace (client fires, server rewinds, damage
  confirmed) remains.
- `--hot-combat-selftest` animation/phase2 failures are concurrent/unrelated.
