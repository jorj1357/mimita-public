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
