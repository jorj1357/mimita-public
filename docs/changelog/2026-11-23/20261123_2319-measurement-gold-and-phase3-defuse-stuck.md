# Measurement gold behavior and Phase 3A/3B trials

## Scope

This session added the measurement/hypothesis gold-behavior document and started
Phase 3A (defuse restart churn) and Phase 3B (wall/stuck measurement). Header
H1/H2 remain deferred. The Phase 3A hysteresis build was still in progress at
the end of the session.

## Measurement gold behavior

- Added `docs/workflows/measurement-hypothesis-driven-improvement.md`:
  falsifiable hypothesis template, pinned before/after rules, gold examples
  (NPC Phase 0/1/2, header fanout, readiness), recursive self-improvement loop,
  anti-patterns, and a minimum evidence checklist.
- Linked it from `docs/ROUTER.md` and `AGENTS.md`.
- Core rule: no behavior or quality claim without a stated hypothesis, a pinned
  before run, a pinned after run, real numbers/units/sample size, and exact
  journal paths.

## Phase 3A defuse churn trials

Observed gen571: 18 defuse starts, 15 interruptions, 3 completions.
The interruptions were all `released`, and operators moved around the 3 m/6 m
range boundaries.

Trials:

1. Hold at the 6 m interaction boundary (gen572): 41 starts, 38 interruptions,
   2 completions. Reverted; it caused boundary oscillation.
2. Hold at the close 3 m objective tolerance (gen573): 19 starts, 19
   interruptions, no completion in the sampled round. `at_objective` still
   toggled true/false while the actor slid across the threshold.
3. Source fix: latch a defuse hold when the designated defuser reaches
   `atObjective`, keep it while `canDefuse` remains true, clear it when the
   defuse state ends or the actor leaves interaction range. The latch is a
   `thread_local` owner-local set in `src/npc/npc.cpp`, not a new `Npc` field, to
   avoid `npc.h` fan-out.
4. Game DLL generation 574 build was requested to validate; it was still
   building at session end. No completion claim.

## Phase 3B wall/stuck measurement

- Gate a stuck episode on a static side-world contact instead of a one-tick low
  displacement. Gen572 reduced `npc.stuck-episode` from 1,372 in the gen571
  window to 553 in a comparable window, while real `npc.stuck-recovery` events
  stayed around 31.
- The episode count now has a different definition; do not compare it directly
  with the old count. It is a cleaner wall-block signal, not a win by itself.
- Gen572 movement metrics after suppressing false stuck episodes: CT 18.62 m/s,
  dash 266.97/min, bhop 15.63/min, air 44.0%, stuck 10.26/min; T 18.25 m/s,
  dash 255.64/min, bhop 13.65/min, air 40.2%, stuck 10.20/min. These were taken
  under concurrent load and are not accepted balance targets.

## Evidence and limits

- Runs: `20261123_215305` (gen572), `20261123_223229` (gen573), plus the earlier
  gen571 run `20261123_205333`.
- Game DLLs: gen572 SHA-256 `2de86cbded18ff9145d34cd95c030bcc69d85674f782616542114cca2931b441`;
  gen573 build published; gen574 in progress.
- Build success is compilation evidence only.
- No gen574 runtime proof yet.
- The measurement document is documentation-only; it does not prove NPC
  behavior.
- Header H1/H2 were intentionally not started in this session.

## Next steps

1. Finish the gen574 build and run a pinned FCS scenario.
2. Verify defuse starts per plant drop from 19–41 toward one per plant.
3. Then continue wall/stuck fixes using the new static-contact stuck signal.
4. Header H1/H2 remain a separate behavior-neutral window.

## Final state at interruption

- Gen574 run `20261123_232235` still showed the churn: 1 defuse start in the
  early window, then 43 starts / 43 interruptions and an explosion after the
  bomb stayed planted. The latch did not solve it.
- Final source attempt: a defuse **travel lock** in `src/npc/npc.cpp`. The
  designated defuser uses the navigation direction toward the bomb, stops
  inside 2.5 m, and suppresses jump/dash/down-dash/freeze while the lock is
  active. A real hazard still releases it.
- Build attempt `game-dll-20261123T234301570308Z-6c7a9c` failed in
  `changed_source_preflight` for `src/network/multiplayer-projectiles.cpp`
  (another agent's pending change). The NPC source was not validated. A
  cross-agent signal was required before claiming anything.
