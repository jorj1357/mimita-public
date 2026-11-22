# issue-93bd29a36d24: One contact path with purpose and damage flags (spec only)

## Human report (verbatim, immutable)

> add that spyknife hitclaim, godball, these specific weapon things these are not good it should all be like 1 single path, 1 single like contact path, that has flags like is it collision, is it damage, is it something else, then if its damage, what kind, hitscan or physical object or etc


## Task linked

### 2026-11-20T04:25:27.251Z — graph-coordination

Task: contact-spec (step 0)

## Scheduled independent review

### 2026-11-22T02:10:05.130Z — build-wait-reviewer

A bounded independent review was queued as wake wake-28e67b8e79f8 after 216 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-22T02:20:17.850Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review (issue-review-sweeper, wake-28e67b8e79f8, 2026-11-22T02:1xZ), read-only; no source/build/close/fix.

OBSERVED FACTS
1. Brief/readiness: issue-93bd29a36d24 is kind=architecture, state=new, owner=graph-coordination, severity/priority=high, one task contact-spec (state open, owner graph-coordination, empty evidence/method, latest_proof null). issue readiness returns ready=false, score 1/8, missing: reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. Linked artifacts are only the issue doc (human quote, task link, and the 2026-11-22T02:10:05Z scheduled-review note); issue reviews=[], reviewers=[], evidence={}, no related_build/related_run.
2. Proof status: no proof row exists for task contact-spec or this issue. Runtime proofs in the brain concern other tasks (collision/melee/network) and none names this issue.
3. Target contract already exists in docs/specs/networking/contact-authority.md: status SPEC ONLY (76-81); ContactPurpose bitmask Collision/Damage/Trigger/Presentation (88-93); target ContactKind incl. Environment (95-103); DamageKind (105-113); ContactCandidate gains purpose, kind, damageKind, optional projectileId/explosionId (116-120); one claim path (122-130); legacy paths enumerated, incl. handleSpyKnifeHitClaim/PACKET_SPYKNIFE_HIT_CLAIM, handleGodballHitClaim/PACKET_GODBALL_HIT_CLAIM, handleGenericMeleeContactBatch, handleNpcDamageRequest/PACKET_NPC_DAMAGE_REQUEST (132-141); migration sequence and the rule that no agent may implement this migration from the spec alone (143-154).
4. No networking migration occurred: the listed legacy handlers/packets are still in source (src/network/server.h:1336,1342,1363; src/network/server-packet-handlers.cpp:909,1052,1542; src/network/server-packet-chat.cpp:257; src/network/packets.h:114-115; src/network/server-packets.cpp:2607,2784-2794,2829). ContactPurpose/ContactKind/DamageKind appear only in the spec and devscripts/tech-debt-audit.py:69, not in src/; the physics MovementContactKind is unrelated.

INFERENCE / UNCERTAINTY
- The written task acceptance (spec defines ContactPurpose, ContactKind, DamageKind, one claim path, legacy adapters, and human pre-review; no networking code changed) is satisfied for the spec-defines clauses and the no-code-change clause, and the spec matches the human request (SpyKnife/Godball become legacy consumers of one path; purpose/damage flags added).
- It is NOT satisfied for human pre-review: no approval quote, clarification, or review by a human/owner is recorded anywhere. Therefore contact-spec should remain open and must not be closed on the spec text alone.
- Documentation ambiguity: ContactKind is defined twice with different members, first at 43-50 (no Environment) and again at 95-103 (with Environment). In one C++ header this is a redeclaration conflict; likely intended current-vs-target, but the doc does not label the first block as current or state that the second supersedes it. An implementer cannot paste both.
- Unknown: whether the human accepts the added Environment kind and Trigger/Presentation purposes as the approved scope.

CONTRARY EVIDENCE
- None found that contradicts the target contract. The only tension is the duplicate ContactKind block above; it does not contradict the request, but it weakens spec clarity.

NEXT DISCRIMINATING CHECK
1. Owner graph-coordination/human records an explicit approval (or requested edits) for the 76-160 target contract, e.g. an issue review/issue quote or issue clarify entry naming the accepted scope. This is the decisive missing artifact.
2. Resolve the duplicate ContactKind: mark 43-50 as current/MovementContactKind context or make 95-103 the single target definition with an explicit supersedes note.
3. Then backfill the seven missing readiness fields (reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk) with evidence pointers. Only after 1-3 should contact-spec be completed; appropriate proof is a spec-only exemption with the approval reference plus confirmation that no src/ contact path changed.
