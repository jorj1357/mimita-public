# NPC diagnostic logging standard

- Date: 2026-11-07
- Type: documentation
- Scope: NPC collision diagnosis and dev-loop runtime evidence guidance

## Changes

- Added `docs/gold/20261107_161115-npc-collision-automatic-diagnosis.md` as a gold example of the owner-level `events.jsonl` detail that exposed a downward NPC recovery and informed a candidate guard.
- Recorded the evidence limits: the follow-up run showed no below-map recovery events, but several NPCs still needed respawning, and the latest fall-sample logging build was not exercised in the same live mode. The wider capsule collision issue remains unresolved and gameplay acceptance is open.
- Updated section 11 of `docs/specs/dev-loop/dev-loop.md` to define this bounded, correlated logging detail as the desired default for gameplay and collision diagnosis, and linked the gold example.

## Validation and remaining work

- Verified both documents exist and the dev-loop section links to the gold example.
- No tests or gameplay run were performed for this documentation-only update.
- Existing note in `docs/architecture/player-npc-systems/npc-movement.md:22` says “todo - explain”; propose replacing it with a short description of the current goal-to-navigation-to-shared-movement ownership and its remaining acceptance gaps.
