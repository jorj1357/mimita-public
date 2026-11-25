# issue-9c1fc3e3e668: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-intrinsic-single-TU-build-cost; identity-reason-dispatch-journal-defects-recur; wake-queued-once)

### 2026-11-25T06:10:22.113Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-40cc83217bc0, activation session-9e66f57a8690). Read-only: no source edited, no build or run started by me, no fix or proof claimed.

**Occurrence (direct).**
- wait_id `game-dll-build:2026-11-25T05:46:30.064Z`; kind `game-dll-build`; stage `game_build`; elapsed 60.544 s vs budget 60.0 s; started 2026-11-25T05:46:30.064Z; request `2026-11-25T05:46:29.660Z-3e6ee776:mode3:game`; source_generation 8. Issue opened 05:47:31.026Z.
- Matching attempt `.dev/build-timings/game-dll-20261125T054630085917Z-48e192.json`: success=true, generation 639, started 05:46:30.085Z, completed 05:47:39.706Z, total_seconds 69.625 (worker 60.812). The build finished 9.10 s after the wait fired, so this is real elapsed time, not a stalled or pending queue.
- Stage split: changed_source_preflight 50.734 s, dominated by compiling the single changed TU `src/network/multiplayer-projectiles.cpp` at 50.552 s; then link 6.234 s (lld 6.047 s), compile of module-identity-provider 1.063 s, publish 0.485 s, object_cache_scan 0.031 s. compiled=2, skipped=643, jobs=10.

**What the wait actually was.** Intrinsic single-TU incremental build cost. The same TU also dominated the immediately prior attempt `game-dll-20261125T053302586316Z-919e8d` (52.701 s). A flat 60 s total-elapsed budget will keep firing on healthy builds whenever this TU changes. This is neither a hang nor a false wait: the detector correctly observed >60 s of real work.

**Acceptance check (issue-9c1fc3e3e668).**
- wait_id, stage, elapsed, evidence: present.
- One issue per wait_id: CONFIRMED. Exactly one issue carries this wait_id (this one).
- Reviewer wake queued exactly once: CONFIRMED. `.dev/agent-wake/events.jsonl` has exactly one `wake.queued` for this issue/wait (`wake-40cc83217bc0`, 05:47:33.393Z) and no second queued wake.
- reason: present but unusable — a mid-JSON tail (`struction","raw":"tree CFG construction ... "truncated":true ... "progress_write_ms":4.459,"record_overhead_ms":5.283}`). Defect tracked in issue-6e08d89b55e3.
- "build/run IDs": NOT accurate. evidence.build_id=`2026-11-25T01:21:28.987918Z` is the last published EXE artifact (`.dev/builds/2026-11-25T01-21-28.987918Z`) and generation=638 is the previously published game generation (attempt 919e8d). The in-flight attempt was `game-dll-20261125T054630085917Z-48e192`, generation 639, and no run_id was carried. First incorrect step: `dev-loop.py:3896` uses `self.latest_build`; `dev-loop.py:3897` uses `self.game_build_generation`, which is assigned only on completion (`dev-loop.py:3259`). Confidence: high (the values match the previous attempt exactly). Tracked in issue-9a4503f6ed68.
- proof_expectation "Dev-loop wait event": NOT proven for this occurrence. No `dev-loop.wait.over_budget` or `dev-loop.wait.reviewer-wake` row exists in any retained 2026-11-25 dev-loop journal (the run journals contain only isolated-run runtime events), and `.dev/state.json` now has `events_path: null` with the root daemon restarted at 2026-11-25T06:03:22.911Z. Tracked in issue-6e2d7b91e7d9 (duplicate record issue-0498cbef4ef3); occurrence appended there.

**Resource context (direct, sidecar 48e192).** status captured, 46 samples @1 s: mean host CPU 90.022 %, peak 99.2 %, minimum available memory 3,605,352,448 B (~3.4 GiB). Live pair resolved: run 20261125_053930, server pid 8680 in bloodtest (5 NPCs, server npc-phase ai_ms 34.28 over 61 ticks), client pid 17384 at ~58.4 fps. Unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation (client mode stale/unreported). Inference only: the ~50 s TU compile ran CPU-bound alongside the live pair; this sample does not prove causation. Gap keys recorded on issue-136d4553ff92.

**Dispatch (direct).** Primary adapter `codex` claimed/launched 05:47:35.941/05:47:35.981Z (pid 11392) and exited; `wake.retry` to `opencode` at 05:54:03.422Z; opencode claimed/launched 05:55:32.315/05:55:32.367Z (pid 22364). Wait to final reviewer launch is about 481.8 s; queue to final launch is about 479.0 s; two `wake.launched` rows exist for one wake id. This is above the 5-minute dispatch target. Tracked in issue-84c584957e65.

**Recurrence and duplicate volume.** This is 1 of 189 `Wait over budget: game-dll-build` build-friction issues (172 in_review, 16 new, 1 assigned) minted per wait_id from 2026-11-20 to now; 8 were created on 2026-11-25 alone, three of them (issue-228f1f95a42d, this issue, issue-8af9b97c9a2a) within 20 minutes. Per-wait_id uniqueness works as designed, but the class is dominated by intrinsic ~50 s single-TU costs and a flat 60 s budget. No duplicate issue was filed by this review; occurrences were recorded on the tracked umbrella issues.

**Set by this review.** related_build=`game-dll-20261125T054630085917Z-48e192`, related_run=`20261125_053930` (from the sidecar's resolved pair). No source edit, no fix, no closure.

**Next discriminating checks (for owners; not done here).**
1. Identity: populate wait evidence from the active game-build identity (started_utc to the attempt id in build-timings) instead of the latest published build/generation (issue-9a4503f6ed68).
2. Reason: derive it from structured stage state, not trailing worker output (issue-6e08d89b55e3).
3. Budget/aggregation: use a per-kind or stall-based budget, or coalesce per (kind,stage), to stop the 189-issue class (issue-9a4503f6ed68 / issue-7d3dcfd7433b).
4. Journal: guarantee the root dev-loop events path exists before recording wait events (issue-6e2d7b91e7d9).
5. Build cost: profile or duplicate-check the include fan-out of `src/network/multiplayer-projectiles.cpp`, which alone costs ~50 s per change (issue-136d4553ff92; header fan-out issue-196c3045b4cb).

**Unknowns.** Whether host CPU (90 %) materially extends the ~50 s TU compile; the client mode independent confirmation is unavailable. Neither is needed to attribute this wait.

## Context

### 2026-11-25T06:10:33.804Z — build-wait-reviewer

related_build: game-dll-20261125T054630085917Z-48e192
related_run: 20261125_053930
