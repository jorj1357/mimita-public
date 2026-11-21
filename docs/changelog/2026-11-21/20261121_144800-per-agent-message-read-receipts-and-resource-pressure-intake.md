# Per-agent message receipts and resource-pressure intake

- Reproduced the broadcast read bug in a temporary coordination store: after agent A read an `all` message, agent B could not receive it.
- Implemented durable per-message/per-agent read receipts, conservative migration of legacy receipts, per-agent unread counts, and dashboard read-by timestamps.
- Updated the coordination contract and added a two-agent inbox/sync regression test.
- Validation: `python -m unittest devscripts.quality.test_agent_coord` — 41 tests passed. No game build or live server stress run was performed; the shared FCS server was active.
- Added issues for AI-run server setup validation under bounded resource pressure and for dev-loop reliability at high CPU/memory use. Attached the screenshot's `Get-CimInstance` process-scan failure to the existing process-inventory issue.
- Shared coordination changes remain subject to human review.

- Additional isolated migration probe passed: named-recipient receipts migrate; legacy broadcast reads with unknown identity remain unread for each agent. The first probe had an inbox-draining assertion-order mistake; corrected probe passed.
