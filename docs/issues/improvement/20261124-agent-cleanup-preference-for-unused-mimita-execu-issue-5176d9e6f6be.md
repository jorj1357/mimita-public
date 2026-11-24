# issue-5176d9e6f6be: Agent cleanup preference for unused MiMITA executables

## Human report (verbatim, immutable)

> can u amke it so there is a agent preference over time  to close running mimita exe processes if u arent using them  to save device resources


## Proof exemption

### 2026-11-24T21:05:13.648Z — unused-exe-preference

Reason: This is a documentation-only agent-policy change. Runtime behavior and resource usage are unchanged; validating the policy against ownership and proof-pin rules is the appropriate evidence.

Paths: AGENTS.md

## Resolution

### 2026-11-24T21:05:19.105Z — unused-exe-preference

Implemented the requested periodic cleanup preference in AGENTS.md with ownership, active-use, supported stop, and evidence preservation safeguards.

Proof exemption: This is a documentation-only agent-policy change. Runtime behavior and resource usage are unchanged; validating the policy against ownership and proof-pin rules is the appropriate evidence.
