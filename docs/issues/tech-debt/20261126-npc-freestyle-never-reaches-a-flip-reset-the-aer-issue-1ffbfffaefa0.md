# issue-1ffbfffaefa0: NPC freestyle never reaches a flip reset: the aerial chase overshoots the ball

## Human report (verbatim, immutable)

> freestyle end reason=timeout resets_this_run=0 ball_height_m=1.01 car_height_m=7.16


## Assignment

### 2026-11-27T04:43:35.401Z — rocket-league-full

Owner: rocket-league-full

## Agent suggestion

### 2026-11-27T05:51:11.653Z — rocket-league-full

2026-11-27 update, generation 778, run 20261127_054938_rl-freestyle3: with the fixed arena the freestyle now leaves the ground (car z 0.325 -> 2.01) but still ends reason=landed after 104 ticks with resets_this_run=0. The car jumps from 2.47 m away, peaks at z=2.01 while the ball top is 2.44, and never arrives above the ball. The phase-2 jump and the ball+0.35 phase-3 threshold are in place; the remaining gap is a closed-loop position/velocity controller that arrives over the ball with enough height and then descends inside the 1.2 m wheel tolerance.

## Agent suggestion

### 2026-11-27T08:19:41.644Z — rocket-arena-physics-hud

Progress 2026-11-27, gen 794, mode 72 run 20261127_081833. Three fixes: freestyle start requires car z<0.9 so the ground jump actually happens; the tracking phase holds jump for 6 ticks like the aerial; self-right no longer overwrites freestyle input (it was zeroing boost whenever chassisUp.z<0.75); flyAtTarget applies the aerial 0.25 boost floor. Result: car telemetry reaches z=2.18 against a ball at z=1.18 (target above ball ~2.16) and vehicle.boost fires at 4.83-5.96 s. Still ends reason=landed, resets_this_run=0 after 93 ticks. Remaining gap is the reset/descent controller (phase 3 must settle onto the ball inside the 1.2 m wheel tolerance), not the climb. Baseline gen 789 run 20261127_080709 peaked below 2.01 m with no boost.
