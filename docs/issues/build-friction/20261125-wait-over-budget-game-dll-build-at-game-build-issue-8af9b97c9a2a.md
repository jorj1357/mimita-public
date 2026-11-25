# issue-8af9b97c9a2a: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-25T06:18:00.541Z — build-wait-reviewer

related_build: game-dll-20261125T055256329120Z-9b2bcf
related_run: 20261125_054920

## Review (confirmed-intrinsic-single-TU-build-cost-1.9x; identity-dispatch-journal-defects-recur; reason-and-wake-queue-pass)

### 2026-11-25T06:18:33.722Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-9c7b2299476c, activation session-08a5c5b087ae). Read-only: no source edited, no build or run started by me, no fix, proof, or closure claimed.

**Occurrence (direct).**
- wait_id `game-dll-build:2026-11-25T05:52:56.306Z`; kind `game-dll-build`; stage `game_build`; elapsed 60.325 s vs budget 60.0 s; started 2026-11-25T05:52:56.306Z; request `2026-11-25T05:52:55.970Z-8c380861:mode3:game`; source_generation 9. Issue opened 05:53:56.693Z.
- Matching attempt `.dev/build-timings/game-dll-20261125T055256329120Z-9b2bcf.json`: success=true, generation 640, started 05:52:56.329Z, completed 05:55:07.766Z, total_seconds 131.437 (worker 113.39). The build kept running about 71.1 s after the wait fired, so this is real elapsed work, not a stalled or pending queue.
- Stage split: changed_source_preflight 96.171 s, dominated by compiling the single changed TU `src/network/multiplayer-projectiles.cpp` at 95.926 s; then link 11.578 s (lld 11.375 s), module-identity-provider compile 1.688 s, publish 0.500 s, object_cache_scan 0.016 s. compiled=2, skipped=643, jobs=10.
- Build note on the attempt: note_id `1d3703603f8d`, agent `opencode-deepseek-v4-1`, "Sweep the projectile spawn segment against corpses for instant corpse.hit feedback", file `src/network/multiplayer-projectiles.cpp`, created 05:52:54.360Z.

**What the wait actually was.** Intrinsic single-TU incremental build cost, but this occurrence is a marked cost regression in that same TU. Read-only comparison of attempt sidecars: gen 638 (`game-dll-20261125T053302586316Z-919e8d`) compiled it in 52.701 s (total 74.75 s), gen 639 (`game-dll-20261125T054630085917Z-48e192`) in 50.552 s (total 69.625 s), gen 640 (this attempt, 9b2bcf) in 95.926 s (total 131.437 s) — about 1.9x the two prior attempts. It is not a hang: the detector correctly observed >60 s of real compile work. Why the TU compile nearly doubled is not isolated here; candidates are the working-tree change content (note 1d3703603f8d) or heavier CPU contention, and the sidecar below is insufficient to separate them.

**Acceptance check (issue-8af9b97c9a2a).**
- wait_id, stage, elapsed, evidence: present.
- reason: PASS for this occurrence. It is readable and structured: `[GAME BUILD] revision=308b20ce... sources=644 jobs=10 priority=high cache=build\game\objects\b10e3a31b3ae8637 seeded=0` (not the truncated mid-JSON tail seen on issue-9c1fc3e3e668).
- One issue per wait_id: CONFIRMED. This issue carries this wait_id and no duplicate was filed by this review.
- Reviewer wake queued exactly once: CONFIRMED. `.dev/agent-wake/events.jsonl` has exactly one `wake.queued` for this wait (`wake-9c7b2299476c`, 05:54:00.283Z) and no second queued row.
- "build/run IDs": NOT accurate. evidence.build_id=`2026-11-25T01:21:28.987918Z` is the last published EXE artifact and generation=639 is the previously published game generation (attempt 48e192). The in-flight attempt was `game-dll-20261125T055256329120Z-9b2bcf`, generation 640, and no run_id was carried. First incorrect step is the same as the peer review: `dev-loop.py:3896` uses `self.latest_build`; `dev-loop.py:3897` uses `self.game_build_generation`, assigned only on completion (`dev-loop.py:3259`). Confidence: high (the values match the prior attempt exactly). Tracked in issue-9a4503f6ed68.
- proof_expectation "Dev-loop wait event": NOT proven for this occurrence. No `dev-loop.wait.over_budget` or `dev-loop.wait.reviewer-wake` row exists in any retained 2026-11-25 dev-loop journal (the run journals contain only isolated-run runtime events), and `.dev/state.json` has an empty `events_path` with `daemon_pid` 4044. Tracked in issue-6e2d7b91e7d9.

**Resource context (direct, sidecar 9b2bcf).** status `partial`, 60 samples @1 s, mean host CPU 95.45 %, peak 100 %, minimum available memory 1,836,306,432 B (~1.71 GiB); sampler thread did not stop before timeout. Runtime alignment: run `20261125_054920`, server pid 13856 in `bloodtest` (5 NPCs, 1 player; npc.server-phase-window ai_ms 59.18 over 41 ticks, total_ms 146.79), client pid 9676 at ~35.95 fps (frame p95 70.66 ms, max 106.9 ms, 36/36 frames over the 4.17 ms budget). `same_run_server_client` true; client actual mode stale/unreported (`mode_unverified`). Unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation. Inference only: the ~96 s TU compile ran CPU-bound alongside the live pair; this sample does not prove causation, and the missing pre-build baseline prevents a matched before/after.

**Dispatch (direct).** Wake queue 05:54:00.283Z; primary adapter `codex` claimed/launched 05:54:03.964/05:54:04.006Z (pid 2104) then exited; `wake.retry` to `opencode` 05:55:32.416Z; opencode claimed/launched 06:12:40.263/06:12:40.279Z (pid 14452). Queue to final launch is 1120.0 s (~18 m 40 s); wait to reviewer launch is about 1123.6 s; two `wake.launched` rows exist for one wake id. This is well above the <=5 min dispatch target and worse than the ~481.8 s on issue-9c1fc3e3e668. Tracked in issue-84c584957e65.

**Recurrence and duplicate volume.** This is another of the `Wait over budget: game-dll-build` build-friction class minted per wait_id (the peer review counted 189 on 2026-11-25 with three inside 20 minutes: issue-228f1f95a42d, issue-9c1fc3e3e668, and this issue). No duplicate issue was filed by this review; occurrences were recorded on the tracked umbrella issues.

**Set by this review.** related_build=`game-dll-20261125T055256329120Z-9b2bcf`, related_run=`20261125_054920` (from the sidecar's resolved pair). No source edit, no fix, no closure.

**Next discriminating checks (for owners; not done here).**
1. Identity: populate wait evidence from the active game-build identity (started_utc to the attempt id in build-timings) instead of the latest published build/generation, and carry the live run_id (issue-9a4503f6ed68).
2. Budget/aggregation: use a per-kind or stall-based budget, or coalesce per (kind,stage), to stop the repeating issue class (issue-9a4503f6ed68 / issue-7d3dcfd7433b).
3. Journal: guarantee the root dev-loop events path exists before recording wait events (issue-6e2d7b91e7d9).
4. Dispatch: keep primary-adapter failure from delaying the reviewer launch beyond the 5-minute target and from emitting two launches for one wake id (issue-84c584957e65).
5. Build cost: explain the gen 640 jump from ~50.5 s to 95.9 s for `src/network/multiplayer-projectiles.cpp`. A matched check would compile the same revision without a live pair, or compare with the change reverted, before blaming CPU contention; also profile or duplicate-check the TU include fan-out (issue-136d4553ff92; header fan-out issue-196c3045b4cb).

**Unknowns.** Whether host CPU (~95 %) materially extends the TU compile versus the source change itself; the client mode independent confirmation is unavailable and no matched pre-build baseline was captured. Neither is needed to attribute this wait.
