# issue-a271b0296faa: Notify agents when the user adds new context

## Human report (verbatim, immutable)

> can u also add a issue that is like whenevr  user sas something all the agents get notified that i said somethign so all are aware of newest context/messages, liek that is a issue for ai agents to figure out best  implenentation or solution for or discussion


## User quote

### 2026-11-20T05:39:23.915Z — brain-check-hook

Via brain-check-hook; supports Clarifies that agents should inspect the shared brain when communicating and append relevant suggestions to existing issues; work ownership should be claimed explicitly.:

> do agents check those and auto assign a owner or discuss and suggest ways to do that problem? can u make it so if a agent sees an issue, like each time it sends a message or something it checks brain e and appends a suggestion or improvement etc

## Triage

### 2026-11-20T05:39:24.517Z — brain-check-hook

Summary: Make agent coordination messages trigger an actionable brain check; support appending issue-linked suggestions/discussion and help agents claim suitable unowned work, then assess broadcast options for latest user context.
Owner: brain-check-hook
Priority: high

## Agent suggestion

### 2026-11-20T05:48:42.129Z — brain-check-hook

Implementation update: devscripts/agent_coord.py now includes a bounded brain_check in sync/send/ask/reply responses, lists current and claimable issues, auto-links a message that mentions one existing issue ID, and appends linked message text to the issue discussion. The --issue option handles messages without an issue ID; issue suggest appends a standalone suggestion. AGENTS.md and the wake prompt tell agents to review the brain_check and use next --claim only when taking ownership. This does not automatically inject each Codex user message into the repo brain or wake idle agents; an external delivery/wake integration remains open. Static syntax validation was performed; no behavioral tests or runtime proof were run.

## Context

### 2026-11-20T14:31:54.104Z — context-bus

reproduction: Tell one agent a new instruction, then ask a second idle agent to sync without repeating the instruction.
expected_behavior: The second agent receives the exact user context at next activation; high-priority context wakes active agents; full quote and linked issue are visible.
acceptance: One context record, one broadcast message, unread delivery to a second agent, high-priority wake evidence, and a context-move audit record.
paths: ['devscripts/agent_coord.py', '.opencode/plugins/mimita-user-context.ts', 'docs/specs/dev-loop/agent-context-contract.md']
evidence: {'implementation': 'user_context bus, OpenCode prompt plugin, sync/brain context, context link/move/lint'}
risk: Duplicate capture, wrong issue routing, or waking too many agents; dedupe by source reference and keep ambiguous routing append-only.
proof_expectation: Two-agent sync test plus a live prompt capture record and high-priority wake request.

## Context

### 2026-11-20T15:06:12.548Z — context-bus

evidence: {'tests': '58 focused tests pass', 'plugin_v1': 'chat.message smoke recorded exact text', 'plugin_v2': 'prompt hook smoke recorded exact text', 'context_records': ['ctx-a05309c9f2cd', 'ctx-9786d8eb621e'], 'lint': 'context lint unlinked=0'}

## Scheduled independent review

### 2026-11-22T07:56:23.882Z — build-wait-reviewer

A bounded independent review was queued as wake wake-4ab506f609c9 after 261 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (changes-requested)

### 2026-11-22T08:20:21.040Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (issue-review-sweeper, 2026-11-22). Read-only: no source edit, build, close, or fix claim. Evidence used: `issue readiness`/`show`, `proof list --issue`, `context lint`, focused tests, and read-only queries of `.dev/agent-coord/brain.db`, `.dev/agent-wake/requests/*.json`, and `.dev/agent-wake/events.jsonl`.

Readiness: ready=true, score 8/8, state triaged, 6 tasks (1 in_progress `message-brain-check`; 5 open under context-bus).

Proof status: `proof list --issue issue-a271b0296faa` returns []. None of the 6 tasks has a task_proof record. The issue `evidence` field is self-reported by context-bus (tests/plugin smokes/lint); it is not a verified proof.

Observed facts:
- Deterministic paths reproduce: `python -m unittest devscripts.quality.test_agent_coord` ran 41 tests OK and `test_agent_wake` ran 3 OK. This includes dedupe+broadcast+ack (`test_user_context_is_deduplicated_broadcast_and_acknowledged`), link/move/lint (`test_user_context_link_move_and_lint`), and missing-field rejection (`test_issue_add_rejects_missing_required_fields`).
- Live capture is active: the brain DB holds 261 user_context rows, 260 `kind=user-context` broadcast messages, and 8283 acks, with `coord.user_context.added` events through 2026-11-22T08:05:10Z. `sync`/`brain_check` return `user_context`, `latest_user_context`, and `unread_user_context`.
- `send` returns `brain_check` and appends an issue discussion when an issue is supplied (agent_coord.py:1262-1287); `sync` returns `brain_check` (agent_coord.py:1195).
- High-priority wake is wired: ctx-fc9b2656cbcd (priority=high) created manual wake requests to two other active agents about 1s after capture. wake-bbaa489ea90f -> issue-review-sweeper claimed+launched 05:17:51Z then status=cancelled; wake-cfcfa4ea6d73 -> fun-cs-npc-objective claimed+launched 05:17:52Z then status=failed ("adapter process exited; reconciled by scheduler").

Counterevidence / defects:
- Over-capture: the OpenCode plugin relays every prompt hook as source=user, including agent-wake launch prompts. 73 contexts from build-wait-reviewer and 32 from issue-review-sweeper begin "MiMITA agent-wake wake-... | agent ..."; this review's own wake prompt is stored as ctx-fc4f22ffd999. These are orchestration prompts, not user utterances, and they now dominate the broadcast stream.
- `context lint` currently reports 153 unlinked contexts of 261 (152 status=new; multiple_primary=0; stale_canonical=0). The recorded issue evidence ("context lint unlinked=0") no longer reproduces, so task `user-context-repair` acceptance ("no unlinked contexts remain") is unmet today.

Inference/uncertainty: Over-capture is high-confidence and explained by the plugin capturing the same `prompt` hook used by wake launches; the fix needs a source filter (e.g., skip when the prompt is an agent-wake launch / MIMITA_WAKE_ID is set). Whether all 153 unlinked contexts must be linked is medium confidence: some may be genuine non-issue chat, but the task's stated acceptance forbids them.

Next discriminating check:
1. Over-capture: sample the next 10 wake-driven activations and confirm each adds a `source=user` user_context row; after a filter, expect 0 new wake-prompt contexts over the next 10 wakes while one genuine human prompt still creates exactly one row.
2. High-priority wake success: add one high-priority context while >=1 other agent is active and capture the wake request reaching status launched and that agent acknowledging within a bounded window (the single sample above ended cancelled/failed).
