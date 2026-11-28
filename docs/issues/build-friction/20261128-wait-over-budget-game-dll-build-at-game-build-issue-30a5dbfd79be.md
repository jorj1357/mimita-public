# issue-30a5dbfd79be: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-28T08:27:37.874Z — build-wait-reviewer

related_build: game-dll-20261128T071157863196Z-8d63e6

## Review (changes_requested)

### 2026-11-28T08:28:07.804Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-63b15223f7f1). Read-only: no source edited, no build/run started, no proof claimed.

**Verdict: changes_requested** — the 60s wait hook fired correctly and queued exactly one reviewer wake, but this wait's dev-loop journal proof and its build identity are incomplete.

**Observed facts**
- Wait `game-dll-build:2026-11-28T07:11:57.814Z` started 07:11:57.814Z and crossed the 60.0s budget at 60.517s; issue created 07:12:58.750Z. Exactly one wrap: `wake-63b15223f7f1` queued 07:13:07.053Z (`wake.queued` count for this issue = 1; only `.dev/agent-wake/requests/wake-63b15223f7f1.json` references it). The exactly-once queue contract holds.
- The waited-on build was real work, not a stall: attempt `game-dll-20261128T071157863196Z-8d63e6` started 07:11:57.863Z, still compiling at the trigger, ended 07:18:54.673Z at total 416.813s (compile 399.172s, 179 compiled / 468 skipped) and FAILED at link (`ld.lld: undefined symbol serverResetPlayerVehicleToKickoff`, referenced by `src/network/server-packet-chat.cpp:642`, defined in `src/network/vehicle-networking.cpp:1282`). Resource pressure: host CPU mean 97.58% / peak 100%, minimum free memory 23.2 MB. The 179-TU recompile follows a `src/host/generated/packet-registry-v1.h` change (header fanout 41), so a full rebuild legitimately exceeds 60s.
- **Proof gap (first missing step):** the run journal `logs/2026-11-28/20261128_061051/dev-loop/events.jsonl` has NO `dev-loop.wait.over_budget` and NO `dev-loop.wait.reviewer-wake` row for this wait. The only wait row is line 189, `dev-loop.wait.issue-failed` at 07:13:37.134Z: `agent-wake.py run --once` timed out after 30 seconds. So proof expectation is met 2 of 3: issue record = yes, wake record = yes, dev-loop wait event = no.
- **Identity defect:** `evidence.build_id` is `2026-11-27T17:45:19.700118Z` — the last published EXE artifact, not this DLL attempt — and `evidence.generation` is null. The true attempt id is recoverable only from the truncated `reason` string. `related_build` was empty; I set it to `game-dll-20261128T071157863196Z-8d63e6`. Dev-loop run id is `20261128_061051`.
- **Reason defect:** `evidence.reason` is a mid-JSON tail (`:57406.0,"build_attempt_id":...`), not a readable stage summary.
- **Dispatch latency:** wake queued 07:13:07.053Z; codex claimed/launched 07:41:06.083Z/07:41:06.107Z then exited; `wake.retry` 07:57:36.925Z; opencode claimed/launched 08:20:38.266Z/08:20:38.291Z (this run). `attempts=2`, two launches for one wait; queue-to-final-launch ~4051s (~67.5 min). The one-launch target did not hold.

**Why the events are missing (root, already tracked):** `build_friction.wake_reviewer` (devscripts/build_friction.py:244-246) runs `agent-wake.py run --once` inline with a hard 30s timeout and no try/except. `dev-loop.check_wait_budget` (devscripts/dev-loop.py:3925-3950) wraps `record_wait_incident` and `wake_reviewer` in one try and adds the wait_id to `reported_wait_ids` before the wake, so the `TimeoutExpired` skipped both wait events and there was no retry even though the issue and wake were both created.

**Cross-references (occurrences appended, no duplicate issue created)**
- Stale build/generation identity + lost over_budget/reviewer-wake events: issue-9a4503f6ed68.
- Mid-JSON reason: issue-6e08d89b55e3.
- Late/retried reviewer dispatch: issue-84c584957e65.

**Next smallest checks (unowned unless claimed)**
1. Make the inline `run --once` launch best-effort (catch `TimeoutExpired`) and emit `over_budget`/`reviewer-wake` independently of inline-launch success.
2. Populate the game-dll wait record with the in-flight attempt id and generation (e.g. `game_build_active_attempt` / `source_generation`) instead of `self.latest_build` / completion-only `game_build_generation`.
3. Replace the raw 240-char `game_build_detail` reason with a parsed stage summary.

**Unknowns:** whether `agent-wake.py run --once` hung only on CPU pressure (plausible: build at 100% CPU) or on a wait/adapter issue; no watchdog log for the inline timeout beyond the 30s exception. Confidence in the facts above: high (direct journal/timing reads); confidence in cache attribution for the link failure: medium (attempt_metrics skipped `vehicle-networking.cpp`; consistent with the recorded cache regression, not independently reproduced here).
