# issue-b09435bb9d9d: Server static map collision double-scales authored boxes and profiles

## Human report (verbatim, immutable)

> vehicle.spawn.settle chosen_z=7.1377 ground_z=6.8027 requested_z=0.2 triangle_center=-42.69,-53.13,6.8


## Agent suggestion

### 2026-11-27T05:51:25.522Z — rocket-league-full

2026-11-27 proof, runtime rl-fuzz run 20261127_053904_rl-fuzz on pinned generation 775: arena2 spawn settle ground_z=-0.01 chosen_z=0.325, world collision 676 triangles, and the settle fuzz finished with kicks=34, wheels_landings=33, roof_rests=1, side_rests=0, tilted_rests=0, worst_settled_up_dot=1.0. The earlier 41 side rests and z=7.14 spawn were symptoms of the double scale. Remaining: verify one GLB map and the client curved-ramp visuals.
