# issue-9b7bf195d8af: A grounded car pressed into a wall or goal frame cannot drive out with full throttle and steer

## Human report (verbatim, immutable)

> car 1 {'position': [-49.57, 36.18], 'speed_mps': 2.45, 'steer_input': -0.9, 'throttle_input': 0.0} and car 2 {'position': [48.02, 34.56], 'speed_mps': 1.86, 'steer_input': -1.0, 'throttle_input': 0.45} stayed in the arena corners for the last 150 s of a 3-minute run


## Agent suggestion

### 2026-11-26T20:18:23.602Z — rocket-league-aerial

Investigated 2026-11-26 (rocket-league-aerial). The two 'pinned' cars were pointed INTO the wall, not trapped by it: run 20261126_175118 car 1 sat at (-49.87, 2.34) with yaw 148.14 deg while its target was at yaw ~32 deg, and car 2 at (44.05, 39.75) with yaw 91.46 deg while pressing +y. Both had wheel_mask 15 and up_dot 1.0, i.e. fully upright. A car cannot yaw at zero speed by design (grounded yaw rate is proportional to speed), so a car aimed at a wall stays aimed at it. Direct counter-evidence that the chassis is not trapped: run 20261126_201711 drove a car at full throttle to (44.4, 26.7) and, with throttle released, it still coasted at 3.9 m/s along the wall (yaw 87.7 deg). Two changes now cover the symptom: the probe controller reverses toward a target behind it and reverses out when it has throttle applied without moving, and it self-rights with air control when its up axis is off vertical. Suggestion: reclassify as a controller/behaviour defect (the bot drove into the wall), not contact-resolution trapping, and keep it open only for the narrower question 'can a car pointing away from a wall drive off with throttle' if a human still observes it.
