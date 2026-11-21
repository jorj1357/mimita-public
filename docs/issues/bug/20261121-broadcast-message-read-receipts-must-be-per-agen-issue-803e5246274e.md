# issue-803e5246274e: Broadcast message read receipts must be per agent

## Human report (verbatim, immutable)

> also ensure that if 1 agent reads a message, just that agent read the message,n ot marking as read for all agnets, like each agent gets it own inbox i think? or leaves only its ownread receipt and time it read it etc


## Agent suggestion

### 2026-11-21T14:47:53.902Z — inbox-and-load-intake

Implementation evidence (2026-11-21): reproduced the bug in a disposable CoordStore (agent A read an all broadcast; agent B then received no message). Added message_receipts keyed by message and agent, conservative legacy receipt migration, per-agent inbox/sync/unread accounting, and dashboard read-by timestamps. Added a regression test covering inbox, sync, independent unread counts, and read-only snapshot receipts. Focused coordination suite: 41 tests passed. No changes were built into or run in MiMITA; because this changes the shared AI coordination contract, leave for human review.

## Agent suggestion

### 2026-11-21T14:49:32.567Z — inbox-and-load-intake

Additional migration evidence (2026-11-21): reopened a disposable legacy-format store after dropping the new receipts table. The named recipient read migrated; an ambiguous legacy all-broadcast timestamp was not assigned to either recipient, and both agents could still read it independently. A first probe accidentally drained agent B inbox while checking a different message; the corrected probe captured each inbox once before assertions and passed all three checks. No production coordination DB was modified.
