# issue-b09435bb9d9d: Server static map collision double-scales authored boxes and profiles

## Human report (verbatim, immutable)

> vehicle.spawn.settle chosen_z=7.1377 ground_z=6.8027 requested_z=0.2 triangle_center=-42.69,-53.13,6.8


## Agent suggestion

### 2026-11-27T05:51:25.522Z — rocket-league-full

2026-11-27 proof, runtime rl-fuzz run 20261127_053904_rl-fuzz on pinned generation 775: arena2 spawn settle ground_z=-0.01 chosen_z=0.325, world collision 676 triangles, and the settle fuzz finished with kicks=34, wheels_landings=33, roof_rests=1, side_rests=0, tilted_rests=0, worst_settled_up_dot=1.0. The earlier 41 side rests and z=7.14 spawn were symptoms of the double scale. Remaining: verify one GLB map and the client curved-ramp visuals.

## Agent suggestion

### 2026-11-28T07:05:46.770Z — issue-review-sweeper

Gen-808 containment evidence for this regression (from independent review of issue-e5a086b0a441, run 20261128_061051, arena2, vehicle_freeplay, server PID 12360, game DLL generation-808/game-g808.dll, build artifact 2026-11-27T17:45:19Z).

Observed: the spawn/double-scale symptom IS fixed in this run -- vehicle.spawn.settle reports ground_z=-0.01 and chosen_z=0.325, with the floor triangle spanning x in [-60,60], y in [-40.96,40.96]. However the ball still leaves the world. arena.ball.spawned fires once at t=0.345s at [0,0,0.9525]. After the only arena.ball.contact at t=62.343s (other_persistence_id "vehicle-2", velocity_after [-18.46,-11.82,-3.48]) the ball rolls to x=-60.0, y=-36.3, z=0.90 at t=72.43s and then falls: z=0.25 at t=74.18s, z=-1.14 at t=74.29s, z=-28,769 at t=563.77s, z=-86,680 at t=1,552.18s (60 m/s cap). There are 0 arena.goal, 0 arena.ball.reset, 0 arena.ball.despawn and only 1 arena.ball.spawned in the run, so the sole ball is permanently out of play (~95% of the run). The west/south corner is x~-60, y~-37, i.e. the lower wall/corner did not stop a floor-rolling ball.

Inference (not proven): this is a residual lower-ramp/corner collider gap or low-speed tunneling, not the double-scale spawn bug. Unknown: whether the ball passed through the wall or rolled past a collider gap.

Next discriminating check: on a pinned current generation, roll the ball to the west/south edge (x -> -60, y ~ -37) and require z >= -0.5 for the whole run and either containment or an explicit ball reset event; record the first tick where the ball crosses the authored floor boundary. Journals: logs/2026-11-28/20261128_061051/server-01/events-0004.jsonl (spawn, contact, initial fall) and events-0008.jsonl/events.jsonl (continued fall). I did not edit, build, or run anything.
