// 2026-09-07T20:59:00Z
# Authoritative Spy Knife Damage

## Branch and pre-existing state

- Branch: not changed by this session; repository branch was not renamed or committed.
- Commit: not changed by this session.
- Pre-existing worktree changes were preserved. They included unrelated account, analytics, movement, replay, gamemode, regression, documentation, and changelog edits. This session did not claim ownership of those edits.

## Change made

The Spy Knife network path was changed from an unused single-hit claim definition to a fixed-size `SpyKnifeContactBatchPacket` containing up to eight contacts. Each contact carries the target ID, historical contact tick, contact ID, target kind, backstab classification, hit position, and direction. The attacker spawn generation is included at batch level.

`src/combat/weapon-spyknife.cpp` now records contact tick/ID/direction with the existing instant predicted hit presentation and flushes queued contacts approximately every 100 ms. `src/combat/weapon-system.h`, `src/combat/weapon-system.cpp`, and `src/engine/engine-tick-combat.cpp` pass remote player replicas into the knife owner so the same collector can target players when no NPC target set is present.

`src/network/server-packet-handlers.cpp` now reads the batch, rejects invalid/stale/non-finite contacts and spawn-generation mismatches, derives damage and knockback from the server weapon definition rather than client damage, rewinds NPC/player target position history, applies authoritative NPC health/knockback or player damage, and emits the existing confirmed damage/NPC damage event path.

## Reasoning

The original client applied NPC health locally and defined `sendSpyKnifeHitClaim` but never called it. The new path preserves immediate effects while giving the server the contact data needed to own final health and damage. Server history already existed for players and NPCs, so it was reused.

## Documents and skill used

- `docs/ROUTER.md`
- `docs/specs/weapons/melee-weapons.md`
- `docs/specs/networking/networking.md`
- `docs/regressions/regressions-v1.md`
- `docs/skills/spec-behavior-review-v1.md`

## Validation

- `git diff --check`: passed.
- `python build_agent.py`: passed with `Status: SUCCESS`; canonical `mimita.exe` build completed.
- `python tools/network_smoke_build.py`: completed successfully without test output.
- No live two-client or client-to-NPC playtest was run in this session.

## Known remaining review items

- The current implementation does not yet maintain a server-side per-attacker contact-ID deduplication set.
- Rejected contacts do not yet have a dedicated rejection packet and prediction rollback ledger; accepted confirmations use the existing confirmed-damage infrastructure.
- Historical validation currently rewinds target pose/position and validates range, but does not reconstruct the full historical swept knife OBB against historical body-part samples.
- The client target selection currently prefers NPC replicas and falls back to remote players when the NPC replica set is empty; simultaneous NPC-and-player target collection should receive focused playtest review.
- Human acceptance remains required for NPC health persistence, player damage, backstab classification, repeated contacts, prediction reconciliation, and network loss/duplication behavior.
