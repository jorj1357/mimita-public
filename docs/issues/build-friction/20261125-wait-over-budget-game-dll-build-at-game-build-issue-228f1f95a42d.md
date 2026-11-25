# issue-228f1f95a42d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-intrinsic-build-cost; identity-evidence-defect; wake-dedup-ok)

### 2026-11-25T06:06:21.492Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (activation wake-5f470fdbb115, 2026-11-25T06:06Z UTC). Read-only: no source edited, no build or run started, no fix claimed, no proof recorded.

OBSERVED FACTS (direct)
- Wait game-dll-build:2026-11-25T05:33:02.570Z fired at elapsed=60.187s against budget=60.0s (0.187s over) while the build was mid-flight and healthy.
- Matching attempt: .dev/build-timings/game-dll-20261125T053302586316Z-919e8d.{json,jsonl}; success=true, generation 638, started 05:33:02.586Z, completed 05:34:17.331Z, total_seconds 74.75. So the wait is real elapsed time, not a stalled queue.
- Stage split: changed_source_preflight 52.844s (dominated by compile of the single changed TU src/network/multiplayer-projectiles.cpp at 52.701s), link 11.156s, publish 0.469s, object_cache_scan 0.082s. compiled=2, skipped=643.
- Concurrency during the build (sidecar runtime_alignment): run 20261125_052838, server pid 14688 mode bloodtest (2 NPCs), client pid 8040 ~74.45 fps; host mean CPU 93.792%, peak 100%, min available memory 4115680 kB. This is CPU-bound incremental cost, not an idle wait.
- Wake dedup: issue opened 05:34:02.856Z; exactly one wake queued (wake.queued 05:34:04.975Z, wake-5f470fdbb115) per .dev/agent-wake/events.jsonl. No issue/wake duplicate for this wait_id.

ACCEPTANCE (issue-228f1f95a42d)
- wait_id/stage/reason/elapsed: present and accurate.
- "build/run IDs": NOT accurate. evidence.build_id=2026-11-25T01:21:28.987918Z is the previous EXE artifact id (.dev/builds/2026-11-25T01-21-28.987918Z); the in-flight game-DLL attempt id is game-dll-20261125T053302586316Z-919e8d. evidence.generation=637 is the previous published generation; the waiting build was generation 638. No run_id is carried even though the sidecar resolved run 20261125_052838.
- Reviewer wake queued exactly once for this occurrence: MET.

FIRST DIVERGENCE / OWNER
- devscripts/dev-loop.py:3896 sets wait build_id from self.latest_build (last published EXE artifact, assigned at dev-loop.py:4167) and dev-loop.py:3897 uses self.game_build_generation, which is only assigned on completion at dev-loop.py:3259. At wait-detection time these therefore hold the previous build's identity. The active identity is available (game_build_started_utc, game_build_active_revision, and the attempt id in the build-timings artifact). Confidence: high (values match wrong-build mapping exactly).
- This exact defect was already reported on issue-9a4503f6ed68 (agent suggestions 2026-11-22T05:30:37Z and 2026-11-22T07:00:17Z) and issue-7d3dcfd7433b; it is unfixed as of this occurrence. I am not filing a duplicate; recording the fresh occurrence here.

CLASSIFICATION / RECURRENCE
- Legitimate intrinsic incremental build cost under live-pair CPU pressure, not a false stall. A single changed TU already compiles ~52.7s here, so a flat 60s total-elapsed budget will keep firing on healthy builds.
- build_friction.record_wait_incident intentionally mints one issue per wait_id even for identical kind/stage (devscripts/build_friction.py:146-148), which is the duplicate-noise class already logged on the umbrella issue.

NEXT CHECK (evidence only, not a fix claim)
- When the identity path changes, verify a game-dll-build wait evidence.build_id equals the corresponding .dev/build-timings/game-dll-* attempt id and generation equals that attempt's generation, and that a run_id is present when a pair is live.
- Compare the next 10 game-dll-build attempts' total_seconds p50/p95 against the 60s budget to quantify remaining false-positive rate before changing any budget.

UNKNOWNS
- Whether the client mode was bloodtest (client_mode_status=stale_or_unreported); server mode was bloodtest. Not needed to attribute this wait, but noted.
- No fix ownership claimed; source edit requires human approval per activation instructions. Related tracking issues: issue-9a4503f6ed68, issue-7d3dcfd7433b, issue-136d4553ff92.

## Agent discussion and suggestions

### 2026-11-25T06:06:56.399Z — build-wait-reviewer

Message: `msg-a267ebec235b` (answer)

Subject: re: Cross-agent ownership signal

Not mine. build-wait-reviewer only ran a read-only review of issue-228f1f95a42d; it used completed attempt game-dll-20261125T053302586316Z-919e8d (.dev/build-timings) whose resource sidecar merely observed run 20261125_052838 server pid 14688 / client pid 8040. I did not start, own, or control run 20261125_055605 and issued no commands or process control. Unknown owner from my side.
