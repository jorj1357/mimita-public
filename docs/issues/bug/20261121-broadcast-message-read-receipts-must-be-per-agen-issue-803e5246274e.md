# issue-803e5246274e: Broadcast message read receipts must be per agent

## Human report (verbatim, immutable)

> also ensure that if 1 agent reads a message, just that agent read the message,n ot marking as read for all agnets, like each agent gets it own inbox i think? or leaves only its ownread receipt and time it read it etc


## Agent suggestion

### 2026-11-21T14:47:53.902Z — inbox-and-load-intake

Implementation evidence (2026-11-21): reproduced the bug in a disposable CoordStore (agent A read an all broadcast; agent B then received no message). Added message_receipts keyed by message and agent, conservative legacy receipt migration, per-agent inbox/sync/unread accounting, and dashboard read-by timestamps. Added a regression test covering inbox, sync, independent unread counts, and read-only snapshot receipts. Focused coordination suite: 41 tests passed. No changes were built into or run in MiMITA; because this changes the shared AI coordination contract, leave for human review.

## Agent suggestion

### 2026-11-21T14:49:32.567Z — inbox-and-load-intake

Additional migration evidence (2026-11-21): reopened a disposable legacy-format store after dropping the new receipts table. The named recipient read migrated; an ambiguous legacy all-broadcast timestamp was not assigned to either recipient, and both agents could still read it independently. A first probe accidentally drained agent B inbox while checking a different message; the corrected probe captured each inbox once before assertions and passed all three checks. No production coordination DB was modified.

## Scheduled independent review

### 2026-11-25T08:16:50.211Z — mode3-latency-continuation

A bounded independent review was queued as wake wake-84065630c3f2 after 716 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (implementation-present; proof-record-missing; acceptance-ambiguity)

### 2026-11-25T08:29:24.966Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review by issue-review-sweeper (wake-84065630c3f2). Read-only: no source, build, or fix changes; nothing closed or claimed.

OBSERVED FACTS (2026-11-25, HEAD includes commit d89884f6 dated 2026-11-21 16:47 -0500)
- Data model now has per-agent receipts: `message_receipts(msg_id, agent, read_at, PRIMARY KEY(msg_id,agent))` at devscripts/agent_coord.py:211-216. The legacy global `messages.read_at` column still exists.
- `sync()` selects unread rows that have no receipt for that agent and inserts that agent's receipt only (agent_coord.py:1126-1150). `inbox()` uses the same per-agent NOT EXISTS filter and returns the caller's own `agent_read_at` (agent_coord.py:1497-1525). `_unread_count(agent)` counts per agent (agent_coord.py:577-583); `status()` uses it (agent_coord.py:4066-4071).
- `coordination_snapshot()` is read-only (no receipt writes) and returns each message's `read_receipts` list (agent_coord.py:3441-3492, 3478-3480). Its `unread` is a sum over all agents, not one viewer (agent_coord.py:3492).
- Dashboard panel renders `unread = " *" if not receipts else ""` and a `read by agent@time` list from the aggregate receipts (devscripts/dev_loop/gui.py:1706-1717). That is a global view; the panel has no selected-viewer identity.
- Migration `_ensure_message_receipts` attributes a legacy read only to a named recipient, and for legacy broadcasts only to an agent who replied; an ambiguous global broadcast read is not assigned (agent_coord.py:560-575).
- Spec agrees for the engine path: "Every agent has an independent read receipt... reading a broadcast never marks it read for another recipient. The dashboard is read-only and shows which agents have read each message." (docs/specs/dev-loop/agent-coordination.md:416-419, 558-562).

INDEPENDENT CHECKS RUN (this review)
- `python -m unittest devscripts.quality.test_agent_coord.AgentCoordTests.test_broadcast_read_receipts_are_independent_per_agent` -> ok, 1 test, 0.083s. The test uses an isolated TemporaryDirectory CoordStore (test_agent_coord.py:27-44).
- `python -m unittest devscripts.quality.test_agent_coord` -> Ran 41 tests in 2.742s, OK. This independently matches the implementer's "41 tests passed" claim.
- Read-only probe of the live store `.dev/agent-coord/brain.db` (sqlite mode=ro): broadcasts=789; legacy broadcasts with global read_at<>''=57; total receipt rows=52683; legacy-read broadcasts with zero receipts=0; legacy-read broadcasts with >=1 receipt=57; direct messages with global read_at<>''=102; agents=219. No write occurred.

INFERENCE / UNCERTAINTY
- Core bug is addressed in the store: one agent reading a broadcast no longer consumes it for another; unread counts and inbox/sync are per agent. Confidence: high for inbox/sync/unread, based on code plus the passing isolated regression test.
- Migration is conservative and, by construction, previously-read legacy broadcasts that cannot be attributed become unread once for recipients. In the live DB all 57 legacy-read broadcasts have at least one receipt, so no message is fully unattributed, but some recipients may see a one-time reappearance. This is an intentional semantic choice, not proven user-acceptable.
- `reply()` writes the replying agent's receipt (agent_coord.py:1473-1478), but the focused regression test exercises inbox, sync, unread counts, and snapshot receipts only; it does not assert the reply path or direct/private non-consumption that the acceptance text names ("through inbox, sync, reply, unread counts, and read-only dashboard snapshots; direct/private messages retain intended recipient semantics"). Those paths are covered indirectly by other tests in the 41-test module, not by an assertion tied to this acceptance.

PROOF STATUS (as found)
- `issue brief` shows tasks=[] and reviews=[]; state was `new`; owner empty; related_build and related_run empty. There is no linked ProofRecord or task for this issue, so a proof-first completion claim is unsupported. The implementation-evidence suggestions are agent prose, not a pinned run or proof entry. This review does not self-approve runtime proof.

CONTRARY EVIDENCE / ACCEPTANCE TENSION
- Acceptance says "GUI labels/counts reflect viewer's own receipt", while the current dashboard is a viewer-agnostic aggregate (global read-by list; "*" only when zero receipts) and the spec says the dashboard shows "which agents have read each message". These can both be reasonable, but they are not the same requirement. The panel cannot show one agent's unread state because it has no selected viewer.

NEXT DISCRIMINATING CHECK(S)
1. Human/product decision: should the dashboard expose a per-agent viewer (using the existing `read_receipts` data) or is the global "who read it" view the accepted reading of the acceptance line? This decides whether any GUI work remains.
2. If the fix is to proceed: add a focused assertion for `reply()` receipt and for direct/private messages not being consumed by a non-recipient, or record a reasoned exemption for those acceptance sub-clauses. Target: the focused test names every acceptance path.
3. Record either a linked task+ProofRecord for the isolated-store regression/migration check, or an explicit proof exemption, so `proof status` reflects the implemented state instead of a blank.
