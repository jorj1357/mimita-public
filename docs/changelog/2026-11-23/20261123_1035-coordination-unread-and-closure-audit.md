# Coordination unread counts and issue closure audit

## Scope

Read-only audit of the dev-loop coordination panel's unread/open issue counters, issue close path, and persistent storage. The request is recorded in `.dev/agent-coord/brain.db` as context `ctx-4954ce5dd811`; the CLI argument preserved line breaks as literal `\\n` separators, so this record is not byte-for-byte identical to the chat quote. The original chat message remains the authoritative verbatim source.

## Findings

- `.dev/agent-coord/brain.db` is SQLite in WAL mode and stores messages, per-agent receipts, issues, and events. `.dev/agent-coord/events.jsonl` is the separate append-only coordination audit stream; issue Markdown under `docs/issues/` is also published. This is local persistence, not a backup or replica.
- The dashboard sums unread receipts for every agent row, including inactive identities; broadcasts count once for each recipient without a receipt. At inspection: 160 agent identities (4 active, 156 inactive), 783 messages (476 broadcasts), 20,403 receipts, and 56,051 unread recipient-message pairs. Only 6 are for active agents; 56,045 are for inactive agents.
- The dashboard snapshot limits open issues to 100, and the GUI displays the returned list length as the open count. The database had 290 issues outside `done`/`cancelled` at inspection. Thus `100 open issue(s)` is a cap, not the total.
- No issue-close control was found in the dashboard GUI. The close operation is `agent-coord issue close`; it requires proven integration/unit evidence or an explicit proof exemption. The five `done` issues in the current DB have resolution events authored by named agents, not `human`/`human-dashboard`; several resolutions explicitly mark duplicate intake. No current evidence indicates the user closed one.
- Issue state and resolution are in SQLite; closure details are also appended to the issue document and recorded in the event stream. The state update and the later document/event append are separate steps, so an interruption between them could leave incomplete cross-store audit detail.

## Recommendation

Show human-actionable messages as the headline metric, or remove the unread headline; keep AI delivery receipts as a separate diagnostic filtered to current/active recipients. Label capped issue lists as `100 shown` and provide a true database total separately. Preserve the SQLite brain and audit/docs, and add a verified backup/restore path if protection from checkout loss or corruption is required.

## Validation

Read current source, coordination contract, dashboard specification, and the live local SQLite database. No code changed and no tests or build were run.

