# issue-c9d8a98dd241: Any agent can ask the user open questions and share answers across agents

## Human report (verbatim, immutable)

> can u make it so agents any agent can ask me any open questions for me to give more detail, like talking to any agent all agents know teh same things that other agents know so any agent can ask me any question


## Triage

### 2026-11-20T07:43:13.531Z — codex_shared_user_questions

Summary: Add a user-facing shared question inbox/notification path and durable answer broadcast; coordinate implementation with context-bus work already active.
Owner: context-bus
Priority: high

## Agent discussion and suggestions

### 2026-11-20T07:43:13.740Z — codex_shared_user_questions

Message: `msg-41f97d147c8c` (status)

Subject: User clarification workflow issue filed

I recorded your requested behavior as issue-c9d8a98dd241 and assigned it to your active context-bus work because the scopes appear to overlap. Source review: issue clarify only overwrites one issue clarification field and appends a detail record; agent sk/eply operate on other agents, not the user. Please include a user-facing open-question path, durable answers, and shared broadcast/sync in your implementation or let me know which non-overlapping slice you want me to take. I will avoid edits to your owned files until we coordinate.

## Context

### 2026-11-20T07:45:46.369Z — codex_shared_user_questions

reproduction: An agent encounters missing product intent or design detail and currently can only store one clarification field on an issue; its agent-to-agent ask cannot prompt the user, while a separate user chat is not automatically represented in shared state.
expected_behavior: Any agent can submit an attributable, durable open question to the user. The user can answer once; the question and answer become shared coordination context visible to all agents at their next sync/activation, without overwriting other questions.
acceptance: Two agents can submit distinct questions; both remain open and visible to the user. A user answer is appended to the correct question with attribution, and every agent can retrieve it from shared coordination state after sync. New user instructions are broadcast through the same shared context path.
paths: ['devscripts/agent_coord.py', 'docs/specs/dev-loop/agent-coordination.md', 'docs/specs/dev-loop/agent-context-contract.md', 'docs/issues/improvement/20261120-any-agent-can-ask-the-user-open-questions-and-sh-issue-c9d8a98dd241.md']

## Agent discussion and suggestions

### 2026-11-20T07:45:46.898Z — codex_shared_user_questions

Message: `msg-41337c9f225d` (broadcast)

Subject: User request: shared questions and knowledge

User directive: any agent should be able to ask the user open questions for detail, and all agents should share the same knowledge from user-agent conversations. Answer this user request by building a durable user-question/answer path visible to all agents at sync; do not overwrite distinct open questions. This message records the current request for all agents.

## Context

### 2026-11-20T07:48:44.179Z — codex_shared_user_questions

evidence: {'observed': 'agent-coord issue_clarify overwrites one issue clarification field and appends a log entry; ask/reply route agent-to-agent; send supports all-agent broadcasts; there is no explicit shared user answer queue/path yet', 'source': 'devscripts/agent_coord.py'}

## Scheduled independent review

### 2026-11-22T14:28:48.752Z — fcs-npc-speed-combat-investigation

A bounded independent review was queued as wake wake-e4f8e54f9733 after 294 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-22T14:46:12.459Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review — issue-review-sweeper, wake-e4f8e54f9733, session-65e7b8797028, work-aea9b8e66eff, 2026-11-22T14:3xZ. Read-only: no source edit, build, runtime, close, or fix claim.

Observed facts (commands run in this checkout 2026-11-22T14:31-14:45Z):
1. Readiness/proof: `issue readiness issue-c9d8a98dd241` = ready=false, score 6/8, missing [proof_expectation, risk], state triaged, 1 task. The only task, `user-question-answer-broadcast`, is `open`, owner context-bus, evidence empty, latest_proof null. No build request or runtime row is linked to this issue. Owner context-bus activity is inactive since 2026-11-20T16:00:47Z.
2. Question intake is single-slot: `issue_clarify` overwrites one `clarification` column and sets state=needs_clarification (devscripts/agent_coord.py:2003-2013, SQL at :2009). A second question on the same issue replaces the machine-readable question even though `_issue_append` keeps both in the issue doc. Distinct issues can each hold one open question.
3. The documented queue command does not return questions: devscripts/agent_coord.py:4683-4687 implements `issue needs-info` as "every issue whose readiness is not ready"; it never selects state='needs_clarification'. Live: `issue needs-info --limit 200` returned 22 issues with states {in_review:17, assigned:4, triaged:1} and 0 needs_clarification, while `issue list --state needs_clarification` returned 7 open questions. So 0/7 pending user questions appear in the command that docs/specs/dev-loop/agent-coordination.md:247-252 tells the user-facing coordinator to run.
4. Spec self-contradiction: agent-coordination.md:218 defines `needs-info` as every open issue that is not ready (matches code); :242-252 says the coordinator checks `issue needs-info` then asks the user the exact open question (matches neither the implementation nor :218).
5. Sync visibility: the sync issue projection (devscripts/agent_coord.py:1189-1192) omits `clarification`, so the 7 questions are not in the activation bundle. `issue brief`/`issue show` and `board` (:3246-3248, SELECT *) do include it. The acceptance asks that every agent retrieve the question+answer from shared coordination state after sync; that currently requires a per-issue brief, not the sync payload.
6. Answer close-out: `issue_quote` (:2662-2675) appends the answer but does not clear `clarification` or change state; `issue_triage` (:1960-1976) also does not clear it, and no code path writes `clarification=''`. After a user answer the issue remains needs_clarification with the stale question and can be re-surfaced.
7. Test coverage: devscripts/quality/test_agent_coord.py:432-445 asserts only that one clarify question is visible in status/board. No test covers needs-info containing a question, two simultaneous questions, or answer close-out.
8. Implemented slice (contrary evidence to "nothing done"): the user-context bus is real and live. `user_context_add` (:2177-2219), sync `user_context`/`user_context_latest` (:1161-1163, :1193-1194), and acks (:2280-2297) exist; recent source=user rows and broadcasts are in the brain. The "new user instructions broadcast through the same shared context path" clause is therefore at least partially satisfied, unlike the question queue.

Inference / uncertainty:
- High confidence: the documented agent-to-user question intake path is non-functional as written; `needs-info` cannot surface the 7 existing questions.
- Medium confidence: the intended user surface may be `board` or `issue brief` rather than `needs-info`; if so, the :247-252 spec step is the defect and the command semantics are correct.
- Medium confidence: single-slot clarification may be acceptable under a one-open-question-per-issue assumption, but the acceptance requires distinct questions not to overwrite; that holds only across distinct issues.
- The issue is not ready (proof_expectation and risk are empty), so even a future fix cannot be marked done without adding those fields.

Unknowns / contrary evidence:
- `board` and `issue brief` expose clarification text, so an agent that already knows the issue can retrieve it; the gap is discovery and close-out, not storage.
- No implementation claim or code change exists to regress; context-bus went inactive, so absence of a fix is an ownership lapse rather than a broken fix.

Next discriminating check (one, bounded):
- Determine the surface the user-facing coordinator actually uses. Run `issue needs-info` vs `issue list --state needs_clarification`, then grep the dev-loop dashboard coordination snapshot/GUI for `clarification`. If no panel lists pending questions, the missing user surface is confirmed at that UI owner; if one exists, only the doc/CLI routing needs repair. Expect 7 pending questions, several older than 2 days (for example issue-35736d408bd4, issue-29115dfd03db, issue-e5a086b0a441).

Confidence: high in the measured divergence; medium in the intended fix location.

## Agent suggestion

### 2026-11-22T14:46:20.008Z — issue-review-sweeper

Implementation recommendation from independent review (issue-review-sweeper, wake-e4f8e54f9733, 2026-11-22T14:3xZ). Evidence: `issue needs-info` returns 22 readiness-incomplete issues and 0 of 7 `needs_clarification` issues; `issue clarify` overwrites a single column; `issue_quote` never clears the question; sync omits `clarification`.

Measurable target: (a) 7/7 current `needs_clarification` issues with non-empty questions are returned by one documented queue command, and the count also appears in `sync`/`brain_check`; (b) after an answer is recorded the issue leaves `needs_clarification` and its `clarification` is empty, so the same question is not re-surfaced; (c) two questions on the same issue both remain machine-readable (0 overwrites), not only in the doc.

Smallest experiment: add one read-only query `SELECT id,clarification,owner FROM issues WHERE state='needs_clarification' AND clarification<>''`, expose it as `issue questions` (or `needs-info --questions`) and include its count/list in sync; add `issue answer <id> --quote ...` that appends the quote and clears `clarification`/sets state=triaged; add one unit test creating two questions on two issues, asserting 2 queue rows, then answering one and asserting 1 row plus triaged state. Run `devscripts/quality/test_agent_coord.py` and the live queue: expect 7/7 today. Keep existing `needs-info` readiness semantics or document the split. No source edit made by this review.
