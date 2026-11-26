# issue-fa0da68ca2b3: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-wait-accepted-known-class-no-duplicate-wake-late-dispatch-recorded-elsewhere)

### 2026-11-26T02:32:23.190Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-793dc730ceba), read-only. No source edit, no compiler/build/worker/second dev-loop/test/runtime started by this agent. Observability-friction record; no gameplay/behavior claim and no fix/closure claimed.

## Acceptance / readiness
- Pre-review readiness ready=true, missing=[], state=new; no fields added (all present).
- Wait record fields present: wait_id game-dll-build:2026-11-26T01:35:04.053Z; stage game_build; elapsed 60.738s vs budget 60.0s; request 2026-11-26T01:35:03.964Z-c594b53f; reason is a clean, complete [GAME BUILD] line (revision=e782f1d1... sources=644 jobs=10 priority=high cache=build\game\objects\b10e3a31b3ae8637 seeded=0), not a truncated JSON tail.
- Reviewer wake queued exactly once for this wait: dev-loop event dev-loop.wait.over_budget + dev-loop.wait.reviewer-wake at 01:36:12.011Z (run 20261126_013130) created this issue (created=true) and queued one wake (wake-793dc730ceba); exactly one .dev/agent-wake/requests/*.json references issue-fa0da68ca2b3. The reviewer-wake launch_result is correctly attributed this time (embeds wake-793dc730ceba / pid 10284 / this wait), unlike earlier misattributed rows.
- Caveat (known, already tracked): evidence.build_id=2026-11-26T01:00:35.913746Z is the previous *published* build and generation=null, and there is no run_id, so the waited-on attempt is only recoverable from build-timings. This is the dev-loop self.latest_build ambiguity already recorded on issue-7d3dcfd7433b, not a new defect.

## Terminal observation (build succeeded; over-budget is real compile cost, not a stall)
Waited-on attempt game-dll-20261126T013504174400Z-8ca897 (run 20261126_013130), timing .dev/build-timings/game-dll-20261126T013504174400Z-8ca897.json:
- status ok, no_op=false, generation 664, revision e782f1d1..., dll build\game\generation-664\game-g664.dll; completed 2026-11-26T01:37:06.235Z, total_seconds 116.234 (worker 94.203), compile 78.718s, link 13.156s, object scan 0.06s, publish 0.375s.
- changed_paths=[src/vehicles/vehicle-system.cpp] with exactly one build note (rocket-league-landing, note_id 6dce3b95c11b); 2 compiled / 643 skipped of 644 sources, jobs=10.
- slowest TU src/vehicles/vehicle-system.cpp = 75.902s (started 01:35:28.062Z, completed 01:36:43.968Z). The budget crossed at ~01:36:04.79Z, while that single changed TU was still compiling -> the 60s crossing is a false-positive for this workload, not a wedge.
- Compiler ftime split for that TU: phase parsing 32.47s (45%), template instantiation 19.58s (27%), opt+generate 26.66s (37%); i.e. legitimate template/parse cost, not I/O.
- Resource sampler: status=partial ("sampler background thread did not stop before timeout"), 92 samples/120.625s, mean host CPU 98.738%, peak 100%, min available memory 1,227,251,712 B (~1.14 GiB). That gap_key build-resource-sampling:game-dll-20261126T013504174400Z-8ca897 is already recorded on issue-2a5c5da53565 (not duplicated here). Correlation only; no causal claim.

## Finding
This is the same known class as issue-347500dfb032 (previous wait) and the umbrella issue-7d3dcfd7433b: the fixed 60s game-dll-build WAIT budget sits below the real single-TU compile time for vehicle-system.cpp (~70-110s on this host), so a normal healthy warm rebuild of that TU predictably produces a "Wait over budget" issue. No new root cause, no stalled build, and no code fix expected from this occurrence. Occurrence added to issue-7d3dcfd7433b; no duplicate issue filed.

## Wake delivery (separate defect, not part of this wait)
The wake was queued once at 01:36:10.215Z but the working reviewer arrived via fallback: codex claimed/launched 01:36:11.920/11.938Z (pid 10284); wake.retry "stale launched request recovered by scheduler" at 02:26:48.876Z; opencode claimed/launched 02:26:50.693/50.742Z (pid 20656). attempts=2, two wake.launched rows for one wait_id, queue-to-final-launch 3040.5s (~50m40.5s), well past the <=5min target. Recorded as a recurrence on issue-84c584957e65, not here.

## Scope boundary / cross-links
- Wait-stage budget and duplicate-wait policy: issue-7d3dcfd7433b (owner launch-latency-investigation). No new proposal; the existing ask (budget curve / dedupe / consolidate) covers this.
- Wake late-dispatch: issue-84c584957e65 (this occurrence appended).
- Build-resource gap 8ca897: already on issue-2a5c5da53565; not duplicated.
- No source edit, no file claim, no fix, no self-approved proof.

## Context

### 2026-11-26T02:33:30.784Z — build-wait-reviewer

related_build: game-dll-20261126T013504174400Z-8ca897
related_run: 20261126_013130
