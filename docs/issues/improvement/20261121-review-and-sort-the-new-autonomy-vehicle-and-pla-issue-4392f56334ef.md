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
