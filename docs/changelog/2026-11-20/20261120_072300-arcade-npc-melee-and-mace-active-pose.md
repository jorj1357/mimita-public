# Arcade melee and Mace active pose

## Changes

- Arcade now skips held-weapon world correction on the server while preserving
  actor capsule collision.
- The server animated-contact pass now keeps NPC damaging melee contacts in
  Arcade and routes them through the shared authoritative contact resolver.
  Arcade skips server-side player attackers to avoid duplicating client claims,
  and skips weapon-world impacts and weapon-clash physics.
- Mode 1 contact remains per fixed tick. Mace action configs set a 10x contact
  damage multiplier for their active pose; client prediction, server claim
  validation, and server physical contacts recognize that configured window.
- Mace renders as solid white while its active pose is selected, then resumes
  its configured texture and tint afterward.
- The melee spec now documents that explicit action multipliers may affect
  Mode 1 damage during the active pose without gating contact cadence.

## Evidence

- DLL build `game-dll-20261120T071159488678Z-9a31ba` succeeded as generation
  398, code hash `900325117e32d6bb9d66175c13b4d050c66f914a17bafe574f04fbbf712d4030`.
- Build notes include note `8a2d3546a1d0`. The run journal reports 16 compiled
  translation units, 626 skipped, and 331.372 seconds total.
- Config parsing confirms both Mace actions contain multiplier `10.0`.
- Isolated runs `20261120_071757` and `20261120_072010` both failed before
  `logger.started`; neither produced server gameplay events. The second run
  used generation 398. These are launch failures, not melee behavior evidence.
- The existing launch-friction issue `issue-040c1b7c5737` received both failure
  records, and its active owner was notified. Melee behavior tasks remain open
  pending a journal-producing run. White-pose rendering has source evidence
  only; no in-game visual review was possible.
