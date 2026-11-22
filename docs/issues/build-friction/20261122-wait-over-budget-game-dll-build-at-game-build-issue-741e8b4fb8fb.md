# issue-741e8b4fb8fb: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass_with_findings)

### 2026-11-22T10:01:08.046Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed build/test wait issue-741e8b4fb8fb (game-dll-build at game_build, wait_id game-dll-build:2026-11-22T09:00:34.850Z).

Verdict: the wait was legitimate, not a stall. No code fix is required for this occurrence.

Evidence checked:
- .dev/build-timings/game-dll-20261122T090034915948Z-7ffb1c.json: success=true, generation=479, total_seconds=85.457, completed 2026-11-22T09:02:00.381Z. Build started 09:00:34.916Z, so the 60.608s wait at 09:01:35 was expected while real work was in flight. NOTE: one saved compiler-input note is attached (bf4e7c4da1e6, cpp-latency-pressure-profile, src/npc/npc-behavior.cpp).
- .dev/build-timings/game-dll-20261122T090034915948Z-7ffb1c.jsonl: translation_unit.completed for src/npc/npc-behavior.cpp at elapsed_ms=54732.434; link.gnu_linker=20914.414ms. Compile+link, not a queue/worker hang.
- .dev/build/current.json + ready.json: generation 479 published, status ok/ready, code_hash 9167cf...8830, revision ab29681...4e3f.
- .dev/agent-wake/events.jsonl: exactly one wake.queued event for issue-741e8b4fb8fb (wake-9bce1e9a483b, WAKE_1063d85014d147e0b0b6c7315822c241) at 2026-11-22T09:01:37.698Z.
- Issue readiness is ready with wait_id, stage, elapsed, budget, build/request IDs and evidence present.

Acceptance result: satisfied. One issue per wait_id, reviewer wake queued exactly once, all required wait fields present.

Finding (diagnostic quality, not a build failure): the issue's reason field is unreadable. It is exactly the trailing 240 characters of the worker's [GAME_FILE] JSON line for npc-behavior (verified: issue reason == last 240 chars of that timing row, MATCH=True). Cause is devscripts/dev-loop.py `_record_game_build_output`, which sets `self.game_build_detail = line.rstrip()[-240:]` for every worker line, including raw JSON payload lines, so the wait issue captures a mid-JSON tail instead of a stage summary or message. Reported separately as tech-debt; not fixed here.

Journal note: this occurrence has no logs/2026-11-22/20261122_011031/dev-loop/events.jsonl (directory absent), so the dev-loop wait event itself is not persisted to a dev-loop journal; the issue evidence plus the build-timings summary/jsonl are the authoritative record for this wait. Reconciling that is part of the separately reported diagnostic work.

Duplicate-volume note: each over-budget wait is keyed by wait_id, so these are distinct occurrences by design, not one issue duplicated. The high volume is systemic (fixed 60s budget vs real compile+link variance), tracked by the existing build-friction work, not grounds to close/replace this record.

## Context

### 2026-11-22T10:01:33.576Z — build-wait-reviewer

related_build: game-dll-20261122T090034915948Z-7ffb1c

## Agent suggestion

### 2026-11-22T10:03:11.808Z — build-wait-reviewer

Follow-up tech-debt filed for the unreadable reason field: issue-6e08d89b55e3 (dev-loop _record_game_build_output stores the last 240 chars of raw worker JSON). The wait itself was legitimate (build generation 479 succeeded in 85.457s); no fix required on this issue.

## Agent suggestion

### 2026-11-22T10:04:36.344Z — build-wait-reviewer

Follow-up tech-debt filed for the unreadable reason field: issue-6e08d89b55e3 (dev-loop _record_game_build_output stores the last 240 chars of raw worker JSON). The wait itself was legitimate (generation 479 succeeded in 85.457s); no fix is required on this issue.
