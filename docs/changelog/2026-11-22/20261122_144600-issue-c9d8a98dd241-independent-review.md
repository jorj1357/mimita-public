# Issue-c9d8a98dd241 Independent Review

# Task

- Task ID: bounded independent review of issue-c9d8a98dd241 (wake-e4f8e54f9733)
- Session ID: session-65e7b8797028
- Work session: work-aea9b8e66eff
- Owner: issue-review-sweeper
- Focus: any agent can ask the user open questions and share answers across agents
- Summary: Reviewed issue readiness, proof state, the issue doc, coordination CLI source, live `needs-info`/`needs_clarification` results, sync/board visibility, answer close-out, and existing test coverage. Appended one evidence review (id 185, verdict `needs-evidence`) and one implementation suggestion to issue-c9d8a98dd241, plus one process proposal to issue-3100aa7b5807. No source, build, runtime, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T14:46:00Z

# Pre-existing changes

- The shared checkout already contained modifications from other agents (including this issue's scheduled-review doc append). This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review and suggestions were written through `agent-coord.py`.

# Requested behavior

Complete one bounded independent review of issue-c9d8a98dd241 without editing source, building, closing, or claiming a fix; record observed facts, inference, contrary evidence, and the next discriminating check, plus a measured process proposal when supported.

# Findings

- Readiness before review: `ready=false`, score 6/8, missing `proof_expectation` and `risk`, state `triaged`, one open task (`user-question-answer-broadcast`, owner context-bus) with empty evidence and no proof row. No build or runtime is linked to the issue; owner activity has been inactive since 2026-11-20T16:00:47Z.
- `issue clarify` overwrites one `clarification` column (`agent_coord.py:2003-2013`, SQL at `:2009`), so a second question on the same issue replaces the machine-readable question; distinct issues each hold one.
- `issue needs-info` is implemented as every readiness-incomplete issue (`agent_coord.py:4683-4687`) and never selects `state='needs_clarification'`. Live: 22 results `{in_review:17, assigned:4, triaged:1}` and 0 of the 7 open `needs_clarification` questions. The spec (`agent-coordination.md:247-252`) tells the user-facing coordinator to use this command to find questions, so 0/7 actual questions are surfaced that way; `:218` defines the same command as readiness, a self-contradiction.
- Sync's issue projection (`agent_coord.py:1189-1192`) omits `clarification`; brief/show and `board` (`SELECT *`) include it. After sync, a question requires a per-issue brief to retrieve.
- `issue_quote` (`:2662-2675`) appends an answer but never clears `clarification` or changes state, and no path writes `clarification=''`, so answered questions remain `needs_clarification` with stale text.
- Test `devscripts/quality/test_agent_coord.py:432-445` covers only single-question status/board visibility; no test covers needs-info questions, two simultaneous questions, or answer close-out.
- Contrary evidence: the user-context bus is live (`user_context_add`, sync `user_context`/`user_context_latest`, acks), so the "broadcast new user instructions" clause is partially satisfied.

# Specification alignment

- Followed the routed proof/coordination expectations: readiness, proof state, and linked artifacts inspected; observed facts, inference, contrary evidence, and next check separated; no behavior completion or fix claimed.
- Preserved all prior issue text; only appended review/suggestion through `agent-coord.py`.

# Exact implementation changes

- None. Append-only issue review and suggestions only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Runtime evidence: read the issue brief/readiness/observe output, `proof status`, the issue doc, coordination CLI source, live `needs-info` and `needs_clarification` listings, sync/board projections, and the existing unit test.
- Review artifact: review id 185 on issue-c9d8a98dd241 (state now `in_review`, reviewers `[issue-review-sweeper]`).
- Process proposal: appended to issue-3100aa7b5807 (documented `needs-info` semantics diverge from the implementation; target 7/7 questions surfaced and 0 doc/CLI contradictions).

# Measured evidence

- Issue age at review: ~2 days (created 2026-11-20T07:43:03Z).
- `needs-info` results: 22, with 0 `needs_clarification`.
- Open `needs_clarification` issues: 7.
- Build/runtime links on the issue: 0.

# Regression review

- Regression entry appended: no.
- Why: no behavior change was made; this was a read-only review with append-only issue updates.

# Human acceptance

- Visual review: not performed by this session.
- Gameplay review: not applicable.
- Still unverified: whether the user-facing coordinator uses `board`/GUI instead of `needs-info`, and whether any dashboard panel already lists pending questions.

# Agent handoff context

- Owner: context-bus.
- Next exact step: (1) confirm which surface the user-facing coordinator uses by grepping the dashboard coordination snapshot/GUI for `clarification`; (2) add one question-queue query and sync count, and an answer command that clears the question and leaves `needs_clarification`; (3) add the two-question/answer unit test; expect 7/7 current questions.
- Do not redo: this readiness/proof inspection or the live `needs-info` vs `needs_clarification` comparison.
- Do not close or mark done without owner/human review; this independent review is not acceptance.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestions.
