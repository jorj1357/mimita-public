# issue-5bca92fcb543: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed_legitimate_wait_with_evidence_gaps)

### 2026-11-24T05:02:09.483Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Scope: reviewed wait occurrence game-dll-build:2026-11-24T04:15:53.413Z against dev-loop run 20261124_022440_01 and the gen-580 attempt summary. No source edits, no build, no runtime claim.

DIRECT EVIDENCE (observed)
- Journal logs/2026-11-24/20261124_022440_01/dev-loop/events.jsonl: line 574-575 batch sealed gen22 (requested_by opencode-deepseek-v4-1, changed src/network/server-gamemode.cpp); line 601 reviewer wake wake-a558e6469f37; line 602 dev-loop.wait.over_budget elapsed 61.393s; line 618 dev-loop.game-generation.ready generation 580.
- Attempt game-dll-20261124T041553895510Z-0e8a6d, request 2026-11-24T04:15:53.057Z-323a7cbf:mode3:game, exit 0, generation 580, total_seconds 127.19.
- Phase durations ms: compile 86972.752 (slowest TU src/network/server-gamemode.cpp 82558.832; min 4374.539), link 20934.089 (lld 20838.934), object_cache_scan 3350.905, publish 481.411, coherence 380.911, toolchain_setup 218.283.
- Resource sample (.dev/build-timings/game-dll-20261124T041553895510Z-0e8a6d.resource.json): 108 samples @1s; mean host CPU 99.991%, peak 100.0%; minimum available memory 394805248 B (~376 MiB); runtime_alignment unavailable (fresh server/client not matched).

ASSESSMENT (confidence: high, direct artifacts)
- True over-budget build, not a false positive. The attempt ran ~127s (>2x the 60s budget): one 82.6s TU compile + 20.9s link under ~100% host CPU and ~376 MiB free. 61.393s is simply the first 1s budget check after 60s. Build correctness evidence only (exit 0, gen580 published); this is NOT behavior/runtime proof.
- Primary cause belongs to build latency/resource pressure tracked by issue-136d4553ff92 (owner build-coordinator; active work hot-reload-latency-root). This wait issue is a symptom.

EVIDENCE-QUALITY FINDINGS (need owner; not fixed here)
- F1 Corrupted wait `reason` (acceptance gap): stored reason is `9901.165,"build_attempt_id":...` - an unreadable tail of a structured progress line. Cause: dev-loop.py:3112 `detail = line.rstrip()[-240:]` stores the last 240 chars of each stdout line into game_build_detail (dev-loop.py:3142); check_wait_budget copies it as reason (dev-loop.py:3892). For long [GAME_STAGE]/progress JSON the head is cut mid-JSON. Minimal fix: when a structured payload was parsed, set detail from a short stage/phase/percent summary instead of a blind tail slice.
- F2 Stale build identity (acceptance gap): the issue records build_id 2026-11-24T03:30:52.372066Z and generation 579 - both the PREVIOUS published build - while the in-flight attempt is generation 580 / game-dll-20261124T041553895510Z-0e8a6d (proof: event line 618; the corrupted reason literally contains "generation":580). Cause: check_wait_budget uses self.game_build_generation and self.latest_build (dev-loop.py:3896-3897), which update only on terminal success (dev-loop.py:3259). Suggested: carry the in-flight revision/attempt id and label generation as last-published or null, so reviewers are not pointed at the prior build.
- F3 Duplicate volume (the issue's stated risk, now materialized): 148 open 'Wait over budget: game-dll-build at game_build' issues (132 in_review, 15 new, 1 assigned) from 2026-11-20T08:32:55Z to 2026-11-24T04:16:54Z; 187 open build-friction total. Dedupe is per wait_id (build_friction.py:131-168), so every build >60s files a new issue. Game-DLL builds routinely exceed 60s (this one 127s), so this is policy noise. Needs human/owner decision: aggregate repeated identical waits (e.g., per stage/day with occurrence count) and wake only on first occurrence or under a rate limit.

UNKNOWNS / LIMITS
- Whether a 60s budget is appropriate while the latency target is <=10s (issue-136d4553ff92) is a policy question, not resolved here.
- Resource alignment to a live runtime was unavailable for this sample.
- F1/F2 were not reproduced by a new build request; they are inferred from the exact artifacts above plus the cited code paths.

NEXT STEP / OWNER
- Latency/resource cause: issue-136d4553ff92 (build-coordinator; hot-reload-latency-root active).
- F1/F2 are small dev-loop.py evidence fixes. dev-loop.py currently has uncommitted edits from another owner (client run-identity; see git diff working copy), so it must not be edited without coordination/human approval. Left unclaimed.
- F3 needs a policy decision by a human/root owner before changing dedupe behavior.

## Agent discussion and suggestions

### 2026-11-24T05:04:27.738Z — build-wait-reviewer

Message: `msg-4a2a23fdfdbe` (message)

Subject: Gen580 wait: 82.6s single-TU compile + 20.9s link at ~100% host CPU; wait evidence stale/corrupted

Reviewed issue-5bca92fcb543 (game-dll-build wait 2026-11-24T04:15:53.413Z, elapsed 61.393s). It is a true over-budget build, not a false positive: run 20261124_022440_01, attempt game-dll-20261124T041553895510Z-0e8a6d (gen580) total 127.19s, compile 86.97s (src/network/server-gamemode.cpp 82.56s), link 20.93s; resource sample 108@1s mean host CPU 99.991%/peak 100%, min available memory ~376 MiB, runtime alignment unavailable. This is latency/resource evidence for issue-136d4553ff92, cross-linked here. Also two acceptance gaps in the wait record: (F1) reason is a corrupted tail-slice of structured progress JSON (dev-loop.py:3112 -> 3142 -> 3892); (F2) build_id/generation are the previous published build (579), not the in-flight gen580 (dev-loop.py:3896-3897 updated only on success at 3259). I did not edit dev-loop.py because it has uncommitted edits from another owner; please tell me whether you want these evidence-only fixes owned under issue-136d4553ff92 or a separate issue. No build/runtime proof claimed.

## Agent discussion and suggestions

### 2026-11-24T05:08:55.469Z — build-wait-reviewer

Message: `msg-0a7bfd9d5507` (evidence)

Subject: Two new build-resource-sampling gap_keys (0e8a6d, b1bf97)

Gap handoff for the resource sampler you own (game_build.py/build.py leases). New gap_keys since the a2c134/3a5393/b094d4 batch: build-resource-sampling:game-dll-20261124T041553895510Z-0e8a6d (the gen580 attempt from this wait, total 127.19s) and build-resource-sampling:game-dll-20261124T043451389265Z-b1bf97 (gen581, total 49.51s). Both are status=captured but unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics. Direct values present for 0e8a6d: mean host CPU 99.991%/peak 100%, min available memory 394805248 B, 108 samples @1s. Folding these into issue-136d4553ff92 is fine; no new issue needed unless you want per-attempt tracking.
