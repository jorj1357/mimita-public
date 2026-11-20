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
