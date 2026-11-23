# issue-ec790f30f5ad: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-live-with-open-defects)

### 2026-11-23T16:39:39.102Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-bc86ede93b34), read-only on 2026-11-23. No source edit, build, close, or fix performed.

OBSERVED (direct journal)
1. The wait fired at 60.15s for game-dll-build:2026-11-23T16:00:36.698Z. Exactly one issue was created (this one) and exactly one wake was queued (wake-bc86ede93b34; one wake.queued at 2026-11-23T16:01:37.511Z in .dev/agent-wake/events.jsonl). The per-occurrence acceptance (one issue, one reviewer wake) is met.
2. Hook path is live: logs/2026-11-23/20261123_144429/dev-loop/events.jsonl has dev-loop.wait.over_budget (16:01:38.982Z) and dev-loop.wait.reviewer-wake (16:01:38.981Z) for this wait_id.
3. This was NOT a stalled build. Attempt game-dll-20261123T160037032706Z-485a05 changed only src/terminal/debug-commands.cpp and finished status=ok: compiled 2, skipped 641, compile_seconds 36.524, link_seconds 21.293, worker_total_seconds 60.977, total_seconds 69.123; generation 563 published/ready 16:01:49.943Z (duration 70.578s). The 60.15s trigger is barely past the flat 60s budget (dev-loop.py:114-115 WAIT_ISSUE_SECONDS) and below this one-file change normal worst case (heavy TU 34.803s + link 21.293s).
4. Required evidence fields are present: wait_id, kind, stage, reason, elapsed_seconds, budget_seconds, started_utc, request_ids, generation, source_generation, build_id.

REPRODUCED KNOWN DEFECTS (cross-linked, not new issues)
A. Wrong identity in wait evidence: evidence generation=561 and build_id=2026-11-23T13:26:39.822193Z (previous published build), while the in-flight attempt is generation 563 / game-dll-20261123T160037032706Z-485a05. Root: dev-loop.py:3259 sets game_build_generation only on completion, but check_wait_budget reads it at dev-loop.py:3879 while status is still building. This is the fresh mismatch pair requested by the NEXT DISCRIMINATING CHECK in issue-9a4503f6ed68 review 217; occurrence appended there.
B. Unreadable reason: evidence.reason is the last 240 chars of a [GAME_FILE] JSON row (":47031.862,"build_attempt_id":..."), from dev-loop.py:3112/3142. Tracked by issue-6e08d89b55e3; fresh occurrence appended.
C. Late reviewer dispatch: wake queued 16:01:37.511Z; codex primary claimed/launched 16:01:38.798/16:01:38.826Z then exited; wake.retry to opencode 16:18:27.553Z; opencode claimed/launched 16:18:29.845/16:18:29.871Z. Delay 1073.173s (~17m53s) after the wait and 1012.360s after queue, above the <=5 min bound. Tracked by issue-84c584957e65; occurrence appended.
D. Policy noise: a flat 60s budget plus per-wait_id issue minting keeps producing Wait-over-budget game-dll-build issues (~150 open; issue-9a4503f6ed68 review 217 measured 127+). This occurrence is one more instance.

RECOMMENDATION (no edit made; issue unowned)
- Treat this as a record of one healthy over-budget wait; no build regression. Route/close it as an occurrence of the tracked cluster after owners act on issue-9a4503f6ed68 (flat budget, identity, dedupe), issue-6e08d89b55e3 (reason), and issue-84c584957e65 (wake latency).
- Candidate owner fixes (need human approval): progress/phase-aware or recent-p95 budget instead of flat 60s; report the active build_attempt_id/generation at trigger time; derive a readable reason from the parsed stage; dedupe by signature as well as wait_id.

PROOF NOTE: no proof record exists for this task. The journal wait events, issue record, and wake request above are the direct evidence; they prove detection/dispatch, not that the 60s policy is correct.
CONFIDENCE: high for build outcome and wake timing (direct journal); high for identity root cause (code + fresh mismatch); medium for the aggregate issue count (not recounted this session).
