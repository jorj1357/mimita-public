<!--
Purpose: Preserve a gold example of owner-level events.jsonl evidence that
revealed an NPC ground-recovery failure and guided a candidate fix.
Audience: MiMITA developers and AI agents diagnosing gameplay from live runs.
Does not: Claim that the general NPC capsule collision problem is fully fixed
or that the correction has human visual acceptance.
-->

# Gold example: the journal exposed an NPC ground-recovery failure

- Recorded: `2026-11-07T16:11:15Z`
- Status: GOLD DIAGNOSTIC EXAMPLE; broader capsule collision fix remains open
- Related specification: `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/architecture/collision/collision.md`
- Related workflow: `docs/workflows/runtime-scenario-validation.md`
- Related guidance: `docs/specs/dev-loop/dev-loop.md`, section 11
- Related regression: `docs/regressions/collisions/npc-closed-block-REG-2026-11-01.md`
- Session record: `docs/changelog/2026-11-07/20261107_110555-npc-ground-recovery.md`

## What the journal revealed

In a natural Fun Counter Strike run on `dust2cyberiav4`, the server journal
`logs/2026-11-07/20261107_154026_01/server-01/events.jsonl` already contained
enough connected facts to identify one concrete failure without a user having
to request a special trace:

- `npc.collision-world.summary` showed 5,463 world triangles copied into the
  NPC collision world, with aligned solid IDs. The world was not empty.
- `npc.root.pushout` recorded actor 100004, tick 4,296, its position before and
  after recovery, the solid ID, and a correction of `(0, 0, -1.9380) m`.
- `npc.below-map.respawned` later recorded the same actor, tick 4,442, at
  `z=2133.2598`, below the map bound `z=2295.3118`.

The event chain identified a specific bad recovery: the nearest enclosing face
was below the floor slab, but the server accepted the downward pushout because
it only checked whether the root was still inside the solid. The later
below-map event confirmed the consequence. Actor identity, ticks, positions,
solid ID, correction vector, and recovery result made the chain actionable.

## How the evidence guided a fix

The recovery owner was changed to reject downward nearest-face corrections,
restore the pre-correction position, and try the existing validated
standing-point recovery. If no valid nearby point exists, it now respawns the
NPC rather than continuing simulation inside geometry. The below-map guard
also responds on the next fixed tick.

This was a promising, evidence-guided correction. A follow-up run on build
`2026-11-07T15:59:33.671985Z` recorded rejected downward pushouts and safe
respawns, with zero `npc.below-map.respawned` events in that run. It did **not**
prove that the capsule movement solver never misses floor contact: eight NPCs
still needed respawn because no safe standing point was found. The later build
added `npc.collision.fall-sample` with capsule movement, candidate, hit, and
penetration data, but that diagnostic build did not receive a same-mode live
run during the session. The general ground-collision fix and human visual
acceptance therefore remain unconfirmed.

## Desired logging level

This is the level of signal the live journal should aim to provide by default
at the behavior owner:

- enough facts to locate the first bad transition without asking the user to
  name a debug flag;
- enough context to distinguish a missing world, missing candidate, missed
  sweep, rejected contact, unsafe correction, and failed recovery;
- correlated actor and tick values plus before/after state and the reason for
  each decision;
- bounded first-failure and transition records with repeated events
  rate-limited or summarized;
- one canonical `events.jsonl`, not a separate unowned debug file.

The journal can reveal and localize a failure before the final fix is proven.
Keep those claims separate: observed failure, diagnosed owner, source change,
successful build, exact-build runtime result, and human acceptance. A candidate
fix can look right while still being untested on the behavior that originally
failed.

## Evidence files

- Original failing run:
  `logs/2026-11-07/20261107_154026_01/server-01/events.jsonl`
- Recovery follow-up run:
  `logs/2026-11-07/20261107_155949/server-01/events.jsonl`
- First-divergence diagnosis, code change, and evidence boundaries:
  `docs/changelog/2026-11-07/20261107_110555-npc-ground-recovery.md`
