# issue-67f2be3d7012: Wire build-friction auto-intake and warm candidate tests

## Human report (verbatim, immutable)

> the goal is to get like , time from agent change to agent prove change works as close to 0 as posisble, so dll builds as close to 0 sec aspossible


## Task linked

### 2026-11-20T01:52:45.603Z — coordination-v2

Task: auto-build-friction (step 0)

## Task linked

### 2026-11-20T01:52:46.315Z — coordination-v2

Task: candidate-dll-pool (step 0)

## Task linked

### 2026-11-20T01:52:46.733Z — coordination-v2

Task: warm-runtime (step 0)

## Triage

### 2026-11-20T01:53:59.211Z — coordination-v2

Summary: Wire build-friction auto-intake, parallel candidate game DLLs, and warm pinned event tests
Owner: coordination-v2
Priority: high

## Assignment

### 2026-11-20T01:53:59.662Z — coordination-v2

Owner: coordination-v2

## Task linked

### 2026-11-20T02:05:25.330Z — coordination-v2

Task: ogi-migration (step 0)

## Task linked

### 2026-11-20T02:05:26.464Z — coordination-v2

Task: live-pinned-proof (step 0)

## Task linked

### 2026-11-20T02:40:48.840Z — proof-system

Task: proof-gate (step 0)

## Agent suggestion

### 2026-11-22T17:36:52.207Z — codex-networking-stall-fix

Related measured agent-change-to-proof friction: DLL attempt game-dll-20261122T173219711862Z-89e54d spent 27.429s in changed-source preflight, then failed because src/npc/npc.cpp changed during snapshot validation. Earlier attempt 171723932165Z-86a612 spent about 202.979s before the same source changed. Networking runtime remains unproven; these are concrete build wait and source stability costs.

## Agent suggestion

### 2026-11-22T22:29:26.291Z — issue-review-sweeper

Process proposal from an independent review of issue-9a4503f6ed68 (2026-11-22T22:2xZ, issue-review-sweeper). Evidence of a repeated avoidable loop in build-friction auto-intake: from 2026-11-21T00:02Z to 2026-11-22T21:33Z, ~12 agent suggestions on issue-9a4503f6ed68 all request the same two changes (raise/derive the wait budget above real build time; aggregate repeated wait signatures instead of one issue per attempt). Current source still has the flat 60s budget (devscripts/dev-loop.py:109-110) and wait_id-only dedupe (devscripts/build_friction.py:131-148), and the coordinator now holds 127 open 'Wait over budget: game-dll-build at game_build' issues plus 149 open total wait issues. The newest live occurrence (22:15:19Z) filed issue-6943764e0d8e for a healthy 94.8s single-TU build. The gap is not diagnosis; it is that repeated same-class suggestions accumulate on an unclaimed issue while the condition keeps reproducing. Proposal: add a read-only 'recurring signature' report to auto-intake/brief/board that flags (a) >=3 open issues sharing the same kind+stage+normalized-title signature and (b) >=3 same-class suggestions on one issue with no owner claim or implementation, then surfaces one next --mode waiting aggregation/triage task. Measurable target: within 7 days of implementation, collapse open identical wait issues to <=2 rolling canonical issues per signature and record median time from first same-class suggestion to owner claim <= 24h. Smallest experiment: build only the read-only counter against coord.db (no auto-merge, no source edits), verify it groups the 127 wait issues into one signature and flags the repeated suggestions on issue-9a4503f6ed68, then decide whether to automate linking/occurrences.

## Agent suggestion

### 2026-11-23T06:29:10.488Z — issue-review-sweeper

Process proposal from the independent review of issue-87f1017235e5 (2026-11-23T06:2xZ, issue-review-sweeper). Distinct from the existing recurring-signature proposal here: this is about reviewer-wake recovery and cold-review evidence, not wait-signature aggregation.

Evidence of a repeated avoidable gap:
- The one reviewer wake for this issue (wake-3caaaeb65666, queued exactly once at 2026-11-20T14:45:53Z) FAILED: codex adapter exited -> wake.retry to opencode -> launched pid 4076 -> wake.failed "adapter process exited" at 2026-11-20T15:27:57Z (.dev/agent-wake/events.jsonl; requests/wake-3caaaeb65666.json status=failed). No review was appended, so the issue sat unreviewed until the periodic sweep first reviewed it ~3 days later at 2026-11-23T06:2xZ.
- The sweep that eventually picked it is triggered by completed-work-session count (dedupe_key issue-review-sweep-364), not wall-clock age, so a failed one-shot reviewer can leave a fresh build-friction issue cold for days.
- Cold-review evidence is fragile: the 2026-11-20 dev-loop session journal that held dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake is gone (logs/2026-11-20 absent, logs/11-20-2026 empty). The review only succeeded by reconstructing from retained .dev/build-timings/game-dll-20261120T144446297866Z-157ad9.* and .dev/agent-wake/events.jsonl.

Proposal (two small parts):
1. On wake.failed for a build-friction reviewer, requeue once with a new wake id and append a "reviewer-unavailable" occurrence to the issue; after a second failure, mark the issue unreviewed and surface it on the board so the sweep or a waiting agent picks it up.
2. At issue creation, persist a compact review bundle next to the record: wait_id, in-flight attempt_id, build-timings path, generation at completion, and the queued wake id -- so a cold review never depends on a session journal that may be pruned.

Measurable target: within 7 days, (a) zero build-friction wait issues older than 24h without either a completed review or an explicit reviewer-unavailable mark, and (b) 100% of cold reviews can cite the occurrence bundle without reading a pruned session journal.

Smallest experiment: build only a read-only board/coord query that lists build-friction wait issues with no issue_reviews row and age > 24h, and check each has a retained .dev/build-timings file. Run it against coord.db now to size the backlog and confirm build-timings retention coverage, then decide whether to add the auto-requeue (no source edits, no auto-merge yet).
