# issue-054ea1bcda9b: Wait over budget: exe-build at starting

## Human report (verbatim, immutable)

> exe-build wait at starting exceeded the wait budget


## Review (confirmed)

### 2026-11-21T14:04:17.172Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded wake review (wake-1c2023057acd, agent build-wait-reviewer). Read-only; no source edits, no file claims, no build started, no fix claimed.

Occurrence is real and single. Wait id exe-build:0:2026-11-21T01:38:16.191Z, kind exe-build, stage starting, reason "starting compiler", elapsed 60.369s vs 60s budget, generation 0, source_generation 0, build_id 2026-11-20T20:20:51.788061Z.

Correlated live context: agent root was active on issue-80dfb9ab9c5c in run 20261121_013815 (game_generation 416) and recorded wait_reason "dev-loop active EXE build setup has exceeded budget; friction tracked in issue-054ea1bcda9b." at 2026-11-21T01:55:43.573Z (coord.activity.set, pid 17924). This ties the friction record to the session that was actually waiting.

Reviewer wake: queued exactly once (wake-1c2023057acd, manual, created 2026-11-21T01:40:03.777Z; one wake.queued event). Delivery was very late: codex claimed 03:11:34.552Z and launched 03:11:35.717Z but exited; fallback retried to opencode at 03:48:06.180Z; opencode claimed/launched 13:50:40Z. Net ~2h10m to first claim and ~12h10m to delivered review, so the "wake once" acceptance is met but the wait went unreviewed for ~12h. This latency belongs to the wake-delivery path (see issue-62ab3daa615d for truncation and the same late-delivery pattern), not to this wait record.

Duplicate check: only this issue carries wait_id exe-build:0:2026-11-21T01:38:16.191Z. Sibling exe-build waits are distinct occurrences/stages (issue-693e63059a7c compiling, issue-723326f11472 setup, issue-7d013b4a6c77 setup). No duplicate issue was created.

Fields complete: readiness ready=true, possible=8, missing=[]; reproduction, expected behavior, acceptance, paths, and proof expectation are all present. Nothing to add.

Root cause/ownership: stage "starting" is EXE-build setup latency, a slice of the broader avoidable-wait program. Cross-link issue-7d3dcfd7433b (Measure every dev-loop wait stage and reduce avoidable latency, owner launch-latency-investigation) and issue-e044a931c54b (Waiting is the enemy). No source fix is claimed in this bounded activation; a fix owner should take the EXE-build "starting" phase measurement separately.

Caveats for the wait-metering owner: (1) the evidence build_id is self.latest_build (last completed build), so it lags the build that was actually waiting; the waiting build is identified by started_utc + generation, not build_id. (2) run 20261121_013815 has no dev-loop/events.jsonl at review time, so this review relies on the issue evidence, the coord activity record, and the agent-wake events rather than the original dev-loop journal; exact per-stage timing needs journal retention if desired.

## Agent suggestion

### 2026-11-21T14:44:53.852Z — root

Measured outcome for the delayed EXE build (2026-11-21): success, compiled 17 / skipped 70, elapsed 2746.712s (~45m47s) in build/build-timing.json. Named phases: compile 1111.074s (~18m31s), link 118.849s (~1m59s), scheduler wait 0.005s, staging 0.267s, PCH 0s. Thus the visible ~18 min aligns with compile phase, but only ~20m10s is attributed; roughly 25m37s remains unaccounted in reported phases. The earlier wait record was stage=starting/reason=starting compiler; exact setup/preparation delay is not measured, so root cause remains unknown. Add phase timing/progress for setup, dependency discovery, queue/permit, compiler startup, each compile batch, and publication, with heartbeat and stale-progress detection. Build succeeded; this timing evidence does not identify a single culprit.

## Scheduled independent review

### 2026-11-25T01:50:29.470Z — daily-review-20261125

A bounded independent review was queued as wake wake-87e118d21daf after 673 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-open; retrospective starting-phase attribution unavailable; observability improvements already tracked)

### 2026-11-25T01:56:36.842Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review - issue-review-sweeper, wake-87e118d21daf, 2026-11-25 UTC. Read-only: no source edited, no build/run started, no fix claimed, no issue closed.

OBSERVED FACTS (re-verified this activation)
- Readiness: ready=true, score 8/8, missing=[]; state in_review, owner empty, 0 tasks. Reproduction/expected/acceptance/paths/proof_expectation all present.
- Wait evidence unchanged: wait_id exe-build:0:2026-11-21T01:38:16.191Z; kind exe-build; stage starting; reason starting compiler; elapsed 60.369s vs 60.0s budget; generation 0; source_generation 0; build_id 2026-11-20T20:20:51.788061Z.
- Wake-once for the original wait: .dev/agent-wake/events.jsonl has exactly one wake.queued for wake-1c2023057acd at 2026-11-21T01:40:03.782Z for this issue (then claimed/launched codex 03:11, retry 03:48, claimed/launched opencode 13:50, wake.failed adapter-process-exited 15:08:08.381Z). The current wake-87e118d21daf is a separate scheduled review sweep (dedupe_key issue-review-sweep-673), also exactly one wake.queued at 2026-11-25T01:50:29.457Z; it is not a duplicate wait record.
- Duplicate check: this wait_id appears only in this issue doc, the umbrella issue-7d3dcfd7433b doc, and the 2026-11-21 changelog. No duplicate wait issue.
- Timing retention: the 2026-11-21 measured outcome is reproducible from the append-only build/build-metrics.jsonl (line 651, timestamp_utc 2026-11-21T02:26:10Z, result success, elapsed_s 2746.712, compiled 17, skipped 70; compile 1111.074s, link 118.849s, scheduler_wait 0.005s, staging 0.267s, pch 0s). build/build-timing.json is single-latest and now holds a 2026-11-25T01:21:26Z build, so only the append-only log preserves the old one.
- The original waiting run logs/2026-11-21/20261121_013815 is still absent (no directory, no dev-loop/events.jsonl), so exact starting sub-phase timing cannot be recovered.

INFERENCE / UNCERTAINTY
- Recompute of the prior root suggestion: 2746.712 - (1111.074+118.849+0.005+0.267) = 1516.517s (~25m16.5s) unattributed, not the stated ~25m37s. The large unattributed gap stands; the derived figure should be ~25m17s.
- Root cause of the 60.369s starting overrun is still unknown. It is EXE-build setup/pre-worker latency (consistent with the game-dll pre-worker wait-clock/setup findings in issue-136d4553ff92) but is not attributable for this occurrence without its journal.
- build_id in the wait evidence is self.latest_build (last completed), lagging the waiting build; the waiting build is identified by started_utc+generation. This is already filed on issue-7d3dcfd7433b (exact suggestion for this issue) and issue-136d4553ff92, so it is a known open gap, not new.
- This issue is an append-only occurrence record; the owner/fix live in the umbrella wait/build-accounting issues. Leaving it open is neither proof fix work is absent nor proof the 2026-11-21 starting-phase gap is closed.

CONTRARY EVIDENCE
- wake.failed for wake-1c2023057acd at 15:08:08Z postdates the delivered review (review id 38, 2026-11-21T14:04:17Z), so the failure event reflects adapter exit after the review, not a missing review. The wake-once acceptance remains satisfied.
- The 2026-11-21 timing evidence is NOT lost: append-only build/build-metrics.jsonl retains it though build/build-timing.json was overwritten.

NEXT DISCRIMINATING CHECK (smallest)
1. On the next exe-build wait over budget, read the single dev-loop.wait.over_budget event plus its issue evidence and pass/fail two predicates: (a) it carries the waiting attempt id and run_id rather than self.latest_build; (b) the starting/setup span is attributed to named pre-worker sub-phases with explicit bounded overhead. One event, no new harness, no rebuild just for this.
2. The 2026-11-21 starting sub-phase cannot be recovered from the absent run 20261121_013815; use build/build-metrics.jsonl line 651 only for the aggregate build total, not the starting sub-phase.

PROCESS REVIEW: The candidate improvements - carry the waiting attempt id instead of self.latest_build, start/attribute the wait clock around pre-worker setup, and retain the waiting build dev-loop events.jsonl - are already recorded on issue-7d3dcfd7433b (exact suggestion naming this issue) and issue-136d4553ff92 (pre-worker setup/sampler gap, missing dev-loop journal, over_budget event retention). No new, non-duplicate process improvement is supported by this sweep, so none appended.
