# issue-dc6792df3815: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-wait)

### 2026-11-21T17:06:39.526Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer in a bounded wake activation (wake-d96e69d7481f, coordination session session-77349442f307). Read-only: no source edits, no builds, no leases.

Issue completeness: readiness ready=true, missing=[], all required fields present (wait_id, stage, reason, elapsed, build/run IDs, reproduction, expected behavior, acceptance, paths, risk, proof expectation, evidence). No clarification needed.

Occurrence is VALID (not a false positive):
- Wait game-dll-build:2026-11-21T16:25:06.256Z started 16:25:06.256Z, budget 60s, elapsed 60.858s.
- Trigger request 2026-11-21T16:25:06.049Z-0c7b1ca9 accepted 16:25:06.366Z; active attempt game-dll-20261121T162506366821Z-0079b9. This wait record DOES carry an active attempt id and the attempt was progressing (contrast issue-4c12dde1d7ba, whose 16:23 review found a false-positive wait lacking an active attempt id).
- dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake fired at 16:26:21.564-565Z; the issue was created exactly once for this wait_id.

First divergence / dominant cause = pre-compile stage, not game compile:
- codegen:resident_state_codegen.py 16:25:06.437 -> 16:25:07.965 (1.53s)
- codegen:packet_registry_codegen.py 16:25:08.631 -> 16:26:32.950 = 84.328s duration. This single stage exceeds the 60s budget by itself.
- manifest_snapshot 6.50s, manifest_validation 0.015s, source_snapshot 7.25s.
- First translation unit (src/actor/player-ai.cpp) did not start until 16:26:51.348Z, ~105.0s after the request; compile/link began only after the wait had already fired.
- The wait reason embeds the packet_registry STARTED event whose elapsed_ms=2266 is that stage's offset from build start, not its 84s runtime. A reviewer reading only reason under-attributes the cause. Recommendation: the wait snapshot should name the currently-running stage and its own elapsed/duration (or the dominant completed stage), separately from game_build's start offset.

Build outcome is separate evidence, not the wait: attempt 0079b9 completed 16:30:45Z success=false, failed_phase=link (worker "link failed in 33.688s", total 338.688s), with undefined references to MimitaNet::resolveServerContact, MimitaNet::queueServerDamageConfirmedEvent, and MimitaNet::sendReloadRequestForWeapon. Those symbols ARE defined in the current source tree (src/network/server-contact.cpp:342, src/network/server-damage.cpp:564, src/network/weapon-runtime-reconciliation.cpp:193), so this is not a missing source definition; it indicates the link ran without the defining objects (skipped/cached TU whose object was not included). The linker also names a cached object with debug paths from the prior snapshot .dev/build-snapshots/game-dll-20261121T155427526457Z-6240d5/source/src/terminal/weapon-commands.cpp while compiling snapshot ...162506...-0079b9, and the prior attempt 6240d5 (15:54:27 -> 16:07:57) also failed at link. This is a distinct object-cache/scan correctness signature, already partly captured by the link evidence on issue-4c12dde1d7ba; recommend attaching it to issue-136d4553ff92 (build-stage accounting) and the existing link-failure evidence rather than opening another wait duplicate.

Recurrence: same "game-dll-build at game_build over budget" signature is recorded many times (e.g., issue-84ec2bf27598, issue-e84cfa28c7f7, issue-c2cebb751fb4, issue-9a9807fa0128, issue-63dccbd8f8b6, issue-2008e743c710, issue-450cfc80a0b9, issue-9aeec1fbae9d, issue-9215d710021d). Recorded here as the current occurrence; no duplicate issue created.

Recommendations (no fix claimed, no code changes):
1. Keep the 60s budget; this wait is valid. Split wait accounting by named stage so a >60s codegen/snapshot interval reports the running stage and its own elapsed.
2. Investigate why packet_registry_codegen.py took 84.3s for packets=78 (I/O, redundant work, or serialized dependency); it is now the dominant pre-compile latency and blocks the first compile.
3. Track the link failure separately: verify the object cache/scan includes every defining TU and that source changes invalidate dependents; add an explicit check that skipped objects still exist and match their source content hash.

Evidence: logs/2026-11-21/20261121_144350/dev-loop/events.jsonl (dev-loop.game-build.stage rows 16:25:06-16:26:47, dev-loop.wait.over_budget, dev-loop.game-build.failed at 16:30:45); .dev/build-timings/game-dll-20261121T162506366821Z-0079b9.json/.jsonl; docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dc6792df3815.md.

Verdict: confirmed-build-wait. Evidence review only; no behavior proof claimed. Suggest state in_review; no owner assigned here.

## Assignment

### 2026-11-27T06:12:49.734Z — rocket-league-input-fix

Owner: rocket-league-input-fix

## Scheduled independent review

### 2026-11-27T07:10:58.636Z — opencode-deepseek-v4-3

A bounded independent review was queued as wake wake-9e996a07ca4b after 835 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-partially-remediated)

### 2026-11-27T07:14:37.854Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review by issue-review-sweeper in a bounded wake activation (wake-9e996a07ca4b, coordination session session-95208fc674de/work-cd5176c0d2fd), 2026-11-27T07:1xZ. Read-only: no source edits, no build, no leases, no close, no fix claim.

ISSUE / READINESS / PROOF
- Readiness: ready=true, missing=[], score=8, state=in_review, tasks=0. The only prior review is record 51 (build-wait-reviewer, 2026-11-21T17:06:39Z, verdict confirmed-build-wait).
- Proof: proof status has no task/proof record linked to this issue. That is expected for this diagnostic build-friction issue (its proof expectation is the dev-loop wait event plus issue and reviewer-wake records, not a behavior event), but no explicit exemption is recorded. The acceptance mechanics (wait_id, stage, reason, elapsed, build/run IDs, evidence, exactly one reviewer wake per occurrence) are present and were satisfied for the original occurrence.

OBSERVED FACTS
1. Original occurrence remains valid; nothing new contradicts record 51. Wait game-dll-build:2026-11-21T16:25:06.256Z, budget 60s, elapsed 60.858s, request 2026-11-21T16:25:06.049Z-0c7b1ca9, attempt game-dll-20261121T162506366821Z-0079b9.
2. The dominant cause named in record 51 (codegen:packet_registry_codegen.py at 84.328s) is now largely gone in fresh evidence. Latest build journals: attempt game-dll-20261127T060005583382Z-9bc595 completed codegen:packet_registry_codegen.py in 5.500s; attempt game-dll-20261127T060412729351Z-9ff9e0 in 7.718s; codegen:resident_state_codegen.py 0.0s (cache hit). Published generation 783 (attempt game-dll-20261127T065859749721Z-de4e7c) reports total_seconds=37.555, compile 27.774, link 4.516, publish 0.445, object_scan 0.043: entire build under the 60s budget.
3. The same over-budget wait signature still fires. dev-loop.wait.over_budget at 2026-11-27T06:05:36Z for wait_id game-dll-build:2026-11-27T06:04:12.334Z, elapsed 62.467s, generation 779, stage game_build; it created issue-8737ccc0152a (state new, no reviewer wake appended yet). In that attempt the pre-compile phases were manifest_snapshot 1.719s, manifest_validation 0.157s, source_snapshot 11.954s, resident_state 0.0s, packet_registry 7.718s, so the marginal ~2.5s overshoot sits in snapshot + changed-TU compile/link, not in one pathological codegen stage.
4. Duplicate-recurrence is real, not hypothetical: issue-dc6792df3815 and issue-8737ccc0152a share the identical human_quote, title, summary, paths, and acceptance, differ only by wait_id/elapsed/attempt, and neither links to the other. Record 51 already listed ~10 earlier same-signature issue IDs.

INFERENCE / UNCERTAINTY
- Inference (confidence medium-high): the specific 84s codegen cause from the original review has been remediated; today's overshoots are a near-budget total-time problem distributed across source_snapshot + compile + link.
- Uncertainty: I sampled 3 recent attempts read-only and did not aggregate all >60s attempts, so "codegen fixed everywhere" is not statistically proven. Whether the 62.5s wait is host contention is unknown: gen 783's postmortem shows peak host CPU 95.51% and minimum free memory 3770 MB, and the resource sidecar reports runtime alignment unavailable ("fresh_server_and_client_journal_evidence_not_matched"), so cause cannot be attributed from this sample.

CONTRARY EVIDENCE
- Generation 783 built in 37.5s, well under budget, so the wait policy does not fire on every build.
- The wait policy works as designed: one issue and one reviewer wake per occurrence.

NEXT DISCRIMINATING CHECK
Aggregate the last 20 game-dll-build attempts read-only from .dev/build-timings/*.json joined to the matching dev-loop events: record total_seconds, source_snapshot, packet_registry_codegen, changed_source_preflight, link, peak CPU/min free memory, and whether wait.over_budget fired. Compute the >60s fraction and the largest phase in those attempts. Hypothesis: >60s builds are now dominated by source_snapshot + changed_source_preflight + link under high CPU, not codegen; falsified if packet_registry_codegen >30s reappears. Smallest experiment needs no new build: parse existing artifacts only.

VERDICT: confirmed-partially-remediated. Evidence review only; no behavior or fix proof claimed. Recommend linking issue-8737ccc0152a to this canonical issue (same human_quote) and leaving state in_review.
