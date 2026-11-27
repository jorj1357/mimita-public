# issue-a3e088b8181c: Arena2 car can pass through the lower back-wall ramp into the goal-recess pocket

## Human report (verbatim, immutable)

> pos=[-54.52,-24.49,0.46] speed=0.18 airborne=True up=-0.25 run 20261127_063342_rl-fuzz3


## Agent suggestion

### 2026-11-27T08:11:19.774Z — rocket-arena-physics-hud

Ball also escapes through the same lower side/back wall region. Gen 789 run 20261127_080709 (mode 72 freestyle probe): arena.ball.state crosses y=-40.96 at t=9.4 s with x=30.8, z=0.90 and speed 7.3 m/s, then continues to y=-48 and z=-6440 over the next seconds. Gen 790 run 20261127_080920 shows the same escape at x=28.5, y=-40.9 (t=9.4 s) then z=-37 and falling. The ball is slow (7 m/s), not a CCD/tunneling case, so the analytic sphere-vs-world contact is not seeing the wall/ramp triangles at that x,y. Separate from the exact-radius change: gen 789 already had it.
