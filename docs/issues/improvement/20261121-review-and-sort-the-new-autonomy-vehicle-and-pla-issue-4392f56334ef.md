# issue-4392f56334ef: Review and sort the new autonomy, vehicle, and platform backlog against the authoritative ROADMAP

## Human report (verbatim, immutable)

> log this stuff in spec or issues and make it so  agnets look at this and review and sort based on like priorities for the project, if we even have thos, and if we dont then we need to ask human/other ai  and collaborate for  a general method of  priprities in terms of the mimta project


## Agent suggestion

### 2026-11-21T04:27:09.023Z — root

Review/sort pass against ROADMAP.md current priority (2026-11-17). Do not invent a second priority system; use the roadmap's five ordered items. Mapping: (1) issue-c8290acca248 automated live release gate for all weapons vs humans/NPCs = supporting gate for roadmap item 1 (Cutover-1/trustworthy live) and item 2 (performance regressions), release-blocking correctness; depends on the shared actor/weapon/contact path and issue-04079f42c6f9; first proof slice = one two-client weapon matrix run with degraded network. (2) issue-0539d967d696 Caravan full end-to-end vehicle proof = roadmap item 4 (creator tools and gameplay content) with a hard dependency on item 2 performance and on the release gate above; must not start ahead of Cutover-1/performance. (3) issue-5534074d9309 VR/cross-platform/video-audio editors/distributed nodes/volunteer hosting = roadmap item 5 long-horizon directions; keep deferred with reason 'roadmap says do not start before Cutover-1 and performance are proven'. Suggested ownership: route c8290acca248 to the existing contact/weapon-gate owner, 0539d967d696 to the vehicle/gamemode owner, and keep 5534074d9309 unassigned as a planning umbrella. Issue is ready (all required fields present) and currently unowned; a bounded agent may claim it to perform the review, but no item should be assigned solely from this suggestion.

## Agent suggestion

### 2026-11-21T05:12:17.220Z — build-wait-reviewer

Roadmap routing review (build-wait-reviewer, bounded wake). ROADMAP.md section 'Current priority — 2026-11-17' is already the authoritative priority method; this issue should attach a roadmap phase/order to new program issues rather than create a second ranking. Proposed ordering and first proofs: (1) issue-c8290acca248 (automatic live multi-client release gate) = phases 1-2 (Cutover-1 trust + performance truth). It is the enforcement instrument for the 2.0.7 melee vs humans regression and depends on issue-04079f42c6f9, issue-503f3e410c01, issue-f8873319f12b, issue-cf9cd77eb056. First proof slice: pinned two-client melee where a human and an NPC each take/land damage with claim -> server accept/reject -> damage -> kill journal lines, plus one deliberately broken path the gate rejects. Defer the all-weapons/all-mode matrix until that base path is green. (2) issue-0539d967d696 (Caravan/vehicle end-to-end) = phase 4 (creator tools and gameplay content), after phase 3 shared project format. First slice is one authored map with car physics/free-play and human+NPC driving, not full Caravan objectives. Keep its dependencies issue-e5a086b0a441, issue-0a64c72f60c8, issue-848a5ba01470, issue-549680e53b26 and the release gate ahead of it. (3) issue-5534074d9309 (VR, cross-platform, distributed volunteer nodes) = phase 5 long-horizon and correctly stays low; ROADMAP forbids starting it before Cutover-1 and performance are proven. Split VR, cross-platform, and distributed storage into separate slices, each with its own pinned proof; it is an umbrella only. Overall claim order: c8290 -> 0539d -> 5534. If a new issue's urgency conflicts with this phase order, record the conflict in the issue discussion for human/another-agent review instead of silently reordering; do not displace ROADMAP or existing owners based on visibility. This issue is unowned and ready; a roadmap-priority owner should triage it and add the phase tag field described in its expected behavior.

## Review (roadmap-phases-routed)

### 2026-11-21T05:16:01.978Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed against ROADMAP.md 'Current priority — 2026-11-17'. No second priority system is needed: ROADMAP is the authority, and this issue should attach phase/order metadata to new program issues. Routing: issue-c8290acca248 -> phases 1-2 (Cutover-1 trust, performance truth; regression enforcement) with first proof = pinned two-client melee claim/accept/damage/kill journal plus one rejected broken path; issue-0539d967d696 -> phase 4 after shared format, first slice car physics/free-play with human+NPC driving in one authored map; issue-5534074d9309 -> phase 5 long-horizon, remains low and must not start before Cutover-1 and performance are proven. Claim order c8290 -> 0539d -> 5534. Conflict rule: log disputed ordering in the issue discussion for human/cross-agent review; never silently reorder or displace existing owners. Remaining gap: issue is unowned and has no roadmap-phase field; a roadmap-priority owner must triage it and add the phase tag required by its expected behavior. No source/build/runtime proof claimed by this review.

## Scheduled independent review

### 2026-11-24T20:38:40.866Z — build-wait-reviewer

A bounded independent review was queued as wake wake-235eea069010 after 628 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-authority-decision)

### 2026-11-24T20:59:49.639Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-235eea069010). Read-only: no source edit, build, close, or fix claim.

OBSERVED FACTS
- Readiness: ready=true, score 8/8, state=in_review, owner empty, 0 tasks; related_build and related_run empty; no proof task exists (agent-coord issue brief/observe/readiness, 2026-11-24).
- Prior review: one review (id 34, build-wait-reviewer, 2026-11-21T05:16Z) routed c8290acca248 -> phases 1-2, 0539d967d696 -> phase 4, 5534074d9309 -> phase 5, claim order c8290 -> 0539d -> 5534, and flagged: "issue is unowned and has no roadmap-phase field".
- That gap is unchanged on 2026-11-24 (628 completed sessions later): owner is still empty and no roadmap-phase field exists on the issue.
- Dependency status now: issue-c8290acca248 state=in_review, owner empty, review 371 verdict needs-evidence (2026-11-24T18:32Z); issue-0539d967d696 state=in_review, owner empty, review 375 verdict needs-evidence (2026-11-24T19:14Z); issue-5534074d9309 state=new, priority low, owner empty, 0 reviews. All three are unowned.
- Schema fact: the issues table has no phase or tags column (devscripts/agent_coord.py:361-383). The generated board table has columns ID/State/Kind/Severity/Owner/Summary/Detail and no priority or roadmap-phase column (docs/issues/README.md:6-7, generated from agent_coord.py:1381-1385).
- Claim-path fact: next_work orders only by priority bucket (high/normal/low), self-ownership, then updated_utc; it never reads ROADMAP phases (devscripts/agent_coord.py:2562-2572). A live `next` today returns issue-6e528c4dc127, not the routed c8290 -> 0539d -> 5534 order.

INFERENCE / UNCERTAINTY
- Acceptance "one validated priority/order list that agents can claim from without asking for a new method" is NOT met. The routing exists only as prose in the review thread; nothing machine-readable stores or sorts by roadmap phase, so agents cannot claim from it.
- "links each to ... existing owners" is not met: all three dependency issues are unowned.
- The gap is a missing bridge, not a priority dispute. ROADMAP.md "Current priority — 2026-11-17" remains authoritative and is not contradicted.
- Whether the fix is a schema `phase` column, a tags convention, or only a board column is a design decision. A schema change touches shared coordination authority, so I cannot infer the intended shape or choose it here.

CONTRARY EVIDENCE
- The prior review is genuine partial progress: it covers all three issues, matches ROADMAP items 1-2/4/5, sets first proofs, and states a conflict rule. That satisfies the prose half of the acceptance.
- No evidence of roadmap-order violation; no disputed ordering found.

NEXT DISCRIMINATING CHECK (smallest)
1. A human/root owner decides the storage shape for "roadmap phase" (schema column vs evidence-JSON/summary convention) and records it here; this is the only blocker between the existing prose routing and a claimable order.
2. Then set roadmap phase + owner on c8290acca248, 0539d967d696, 5534074d9309 and run `agent-coord.py next --claim` as a non-owner to confirm the claim order matches ROADMAP. If it does not, the next check is the next_work sort key, not the issue text.
Do not add a second priority system and do not close this issue on prose routing alone.

Acceptance remains open. Ownership is unclaimed; no source/build/runtime proof is claimed by this review.
