# issue-04079f42c6f9: Visible melee contact passes through and is not accepted

## Human report (verbatim, immutable)

> like melee hit like the weapon just passes thru the enemy on my screen i want it to be a hit each time i see me as the client i see it hit them 99.99% of the time its a hit prediciton is correct 99.99% of the time


## Triage

### 2026-11-20T02:43:15.402Z — melee-networking

Summary: Add blade-capsule fallback when configured damage boxes miss, preserve same shape in claim, add contact-source evidence, verify with NPC/PvP.
Owner: melee-networking
Priority: high

## Task linked

### 2026-11-20T02:43:16.159Z — melee-networking

Task: melee-visible-contact (step 0)

## Review (needs-evidence)

### 2026-11-20T03:57:06.906Z — melee-networking

Reviewer: melee-networking

Pinned run 20261120_034533 used artifact 2026-11-20T02:29:08.406738Z and game generation 393. Client produced 171 claims/123 batches; server journal had zero melee.contact.batch/claim decisions. First divergence is server packet dispatch/receipt, not final damage. Earlier report artifact was stale; current journal confirms 171 unanswered claims. Need add packet-dispatch evidence and rerun after build.

## User quote

### 2026-11-20T06:49:11.798Z — juggernaut-npc-stuck-shooting

Via juggernaut-npc-stuck-shooting; supports unspecified:

> also need to ensure that melee contacts are working taht neeeds work

## Agent suggestion

### 2026-11-20T06:53:41.818Z — juggernaut-npc-stuck-shooting

Arcade/melee report should reuse this existing high-priority visible-contact issue. Prior reviewer found run 20261120_034533: 171 client claims/123 batches but zero server melee.contact.batch/claim decisions, putting the first evidence gap at server packet dispatch/receipt; needs a current pinned reproduction. Also cross-reference Arcade weapon-world-collision requirement to issue-503f3e410c01, which already owns the user's broader melee/Arcade weapon request.

## Scheduled independent review

### 2026-11-22T00:29:43.172Z — cpp-latency-pressure-profile

A bounded independent review was queued as wake wake-53fb4b14a7bc after 204 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-22T01:02:21.990Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper (bounded independent review, wake-53fb4b14a7bc). Read-only: no source edits, build, run, close, or fix claim.

Issue/readiness: issue-04079f42c6f9 is in_review, readiness=false (score 1/8; missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk). Task melee-visible-contact is open with empty evidence, and task_proofs has no record for it. The only prior review (melee-networking, 2026-11-20T03:57Z) cited run 20261120_034533, whose journals are no longer present under logs/.

Observed facts (journals):
1) Latest melee-claim run found: 20261121_224058 (build/artifact 2026-11-21T02:26:50.306117Z, .dev/current exe; run created 22:41:00Z). client-01/events.jsonl contains 4 melee.contact.batch.dispatch, 4 melee.contact.batch.received, and 4 melee.contact.claim.rejected, 0 accepted at ticks 16373-16376 (2026-11-21T23:03:15Z). All 4 rejected at stage=occlusion, reason=world_occluded, target=1000, distance 3.933/4.840/4.875/5.170 m. Two melee.contact.fx events exist at t~803 s (22:54:42-43Z), ~9 min before these contact ids and with no contact_id field, so they do not correlate to the rejected claims. server-01/events.jsonl (pid 27816, process=server) has zero melee.contact events; the 4 rejections are in client-01/events.jsonl but carry process=server, pid=15340, instance_id=client-01. run.json currently lists server pid 27816 and client pid 24192; pid 15340 wrote 10,392 events into the client-01 journal. Event content shows server-side rejection, but journal ownership is ambiguous and should not be used as a clean client/server proof join.
2) Contra-evidence: 20261120_232155 (gen416) server-01 journal has 49 batch.dispatch + 49 batch.received, 18 claim.accepted, 23 rejected (15 friendly_fire, 5 target_generation_changed, 3 target_dead), 0 world_occluded. Accepted claims are all target_kind=npc, 2 with current_reach_fallback=true, distances 0.83-6.19 m, and 4 killed=true. This shows dispatch/receipt/acceptance worked on that artifact/map and contradicts the prior review's blanket "zero server decisions" as a current-state claim.

Inference: In the newest observed melee run the first divergence is not packet dispatch/receipt (batches were dispatched and received) and not the shape/reach gate; it is server occlusion validation rejecting all four claims. That is consistent with the user's report (client sees contact, server does not accept), but it does not prove the client rendered those specific contacts, because no client visible-contact event is joined by contact_id and the journal ownership anomaly prevents a clean client/server correlation. The older accepted run is contrary evidence that the defect is not continuous: it may be weapon/map/artifact-dependent, or the 224058 claims may be false-positive client predictions.

Uncertainty: target=1000 identity (npc/player/actor id range) is unconfirmed from the reviewed lines; the world_occluded rejection does not log attackerContactPos, hitPos, blockedPos, or blockedNormal, so it is not possible to tell whether the occlusion ray legitimately hits world geometry or starts inside/behind a client/server mesh discrepancy. The earlier cited run 20261120_034533 is gone, so its "171 unanswered claims" cannot be re-verified.

Next discriminating check (bounded, owner melee-networking): on a fresh pinned run using the same weapon/map where the user sees a contact, add bounded owner-level fields to melee.contact.claim.rejected (attackerContactPos, hitPos, blockedPos, blockedNormal, target kind) and pair one client visible-contact event with the claim by contact_id. Then determine for each world_occluded rejection whether the ray starts inside a collider or hits a different mesh than the client rendered. Success criterion: either the false occlusion is shown and fixed, or the client claim is shown not to correspond to a rendered contact. No fix or closure claimed here.
