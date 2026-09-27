# Force Punch attack timeline architecture review

Date: 2026-09-26

## Scope

Architecture review followed by the smallest implementation of the 15-tick startup, 8-tick active, and 15-tick recovery Force Punch MVP.

## Findings

- Swordsword already contains startup/active/recovery states, but the timing is duplicated between the client implementation in `src/combat/weapon-swordsword.cpp` and the authoritative server implementation in `src/network/server-physical-contact.cpp`.
- Both Swordsword implementations advance float-second timers and transition after adding `dt`; they do not provide exact fixed-tick counts.
- The server receives attack intent before its fixed-tick simulation step, then runs physical contact after player/NPC simulation in the same 60 Hz tick. This gives a deterministic place to count the attack-start tick.
- Force Punch contact and damage already belong to the shared server physical-contact owner. The new timing owner should only decide whether the contact shape is enabled.

## Decision

Generalize the timing portion into one small fixed-tick `AttackTimeline` owner. Do not put collision, damage, target tracking, or knockback in it. The first implementation should use it for server-authoritative Force Punch and migrate Swordsword's server timing seam without changing Swordsword collision behavior. Client animation and presentation remain outside this MVP.

## Deferred

Startup/active/recovery poses, movement commitment, client presentation migration, and broader weapon JSON cleanup remain follow-up work after the server timeline is proven in-game.

## Evidence status

Implemented `AttackTimeline` as the shared fixed-tick phase owner. The authoritative server starts it for Force Punch and Swordsword, and the existing physical-contact shape is enabled only while the timeline is active. The deterministic test passed with 15 startup, 8 active, 15 recovery, and 8 contact ticks. The canonical build completed successfully and linked the new timeline object. Live server/client and human in-game acceptance remain required.
