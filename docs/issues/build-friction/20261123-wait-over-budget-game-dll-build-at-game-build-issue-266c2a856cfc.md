# issue-266c2a856cfc: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (wait-confirmed; identity-defect-reproduced; latency-recurred)

### 2026-11-23T16:23:15.649Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded wait review (wake-6f2f936370f3). Read-only: no source edit, no build requested, no run started, no fix claimed, no issue closed.

VERDICT: wait-confirmed; underlying build healthy; known wait-evidence identity defect reproduced; reviewer dispatch latency recurred.

OBSERVED FACTS
1. The wait is real, not a stalled loop. Wait record: wait_id game-dll-build:2026-11-23T15:24:55.624Z, kind game-dll-build, stage game_build, elapsed 60.82s vs budget 60.0s. Dev-loop journal logs/2026-11-23/20261123_144429/dev-loop/events.jsonl line 180 = dev-loop.wait.over_budget at 15:25:58.039Z; line 179 = dev-loop.wait.reviewer-wake at 15:25:58.038Z (issue-266c2a856cfc, wake-6f2f936370f3, queued=true).
2. The in-flight attempt was genuine and succeeded: dev-loop.game-build.started at 15:25:05.354Z, build_attempt_id game-dll-20261123T152455729876Z-62f5f6, request 2026-11-23T15:24:55.284Z-8a837006:mode3:game, 6 changed compiler inputs (engine-tick.cpp, npc.cpp, perf-frame.cpp, physics-collision-dispatch.cpp, debug-commands.cpp, npc-cpu-probe.h). .dev/build-timings/game-dll-20261123T152455729876Z-62f5f6.json: started 15:24:55.730Z, completed 15:26:43.708Z, success=true, total_seconds=107.935, generation=561, compile_seconds=81.883, link_seconds=9.646, slowest compile src/npc/npc.cpp 50.792s. The wait fired 0.82s past budget during a build that legitimately needed ~108s.
3. Compiles were ~3.5x slower than the 13:26 baseline for the same files (npc.cpp 14.549s at 13:26 vs 50.792s here) and the next attempt game-dll-20261123T155737827366Z-ad165e failed at changed_source_preflight with debug-commands.cpp at 85.397s vs 10.849s baseline. This is consistent with host CPU/build-resource contention in the 15:24-16:00 window (inference; no CPU/memory sample was captured, so pressure is not directly observed).
4. Mechanism acceptance is met for this occurrence: exactly one issue record (coord.issue.opened seq 10757 at 15:25:56.570Z) and exactly one reviewer wake queued for this wait_id (wake.queued 15:25:57.335Z). No duplicate wake.queued exists for this wait_id.
5. Known evidence-identity defect reproduced on a fresh pair (the next discriminating check requested in the issue-9a4503f6ed68 review): issue evidence records generation=null and build_id=2026-11-23T13:26:39.822193Z (the previous EXE build), while the actual in-flight attempt is generation=561 and build_attempt_id game-dll-20261123T152455729876Z-62f5f6. The issue acceptance requires build/run IDs, so that part is not met. Same defect as issue-9a4503f6ed68 item 3.
6. evidence.reason is not a meaningful stage reason: it is a truncated tail of a compiler progress JSON line (tree CCP ... progress_write_ms/record_overhead_ms). It names no stage owner and no slow file.
7. Per-attempt duplication reproduced in the same run: issue-8a2e0ac7236b (wait 15:57:37.550Z) and issue-ec790f30f5ad (wait 16:00:36.698Z) were created for the same stage within 36 minutes. This matches issue-9a4503f6ed68 item 4 (127+ identically titled wait issues).
8. Reviewer dispatch latency recurred (issue-84c584957e65 class): wake-6f2f936370f3 queued 15:25:57.335Z; codex claimed/launched 15:25:57.912Z/15:25:58.000Z; wake.retry to opencode at 15:58:40.915Z ("previous adapter process exited"); opencode claimed/launched 16:01:38.666Z/16:01:38.737Z. Queued-to-launch delay 2141.402s (~35m41s), far beyond the proposed <=5 min bound. Appended as an occurrence on issue-84c584957e65.

INFERENCE
- The over-budget hook is working as designed for a healthy but slow build. The actionable defects are the flat 60s budget, stale wait evidence identity, garbled reason, per-attempt issue flood, and wake dispatch latency. All already have owners (issue-9a4503f6ed68, issue-84c584957e65, issue-7d3dcfd7433b), so no new code-fix issue is needed for this occurrence.
- A build that touches npc.cpp is already near the 60s threshold on this host without contention (npc.cpp ~50.8s + link ~9.6s + setup/scan ~1.7s is about 62s), so the flat threshold will keep firing for legitimate changes.

UNKNOWNS
- The exact cause of the 3.5x compile slowdown is unmeasured; contention is inferred only.
- Whether the composed reason should be the last [GAME BUILD] line or a structured stage/file value is a design choice for the owner.

NEXT DISCRIMINATING CHECK
- After the identity fix, re-read a fresh wait issue and confirm evidence.generation equals the in-flight dev-loop.game-build.started generation and evidence carries the journal build_attempt_id for the same wait_id.
- Confirm issue-84c584957e65 behavior: one wake.queued to one healthy adapter launch within 5 minutes, with no ~30 minute codex-to-opencode retry gap.
