# issue-41547751ca1c: Build a Rocket League-style mode through staged playable acceptance

## Human report (verbatim, immutable)

> The first ... goal ... is I can play in a client server architecture version of Rocket League in this game mode and I can fly around, I can air roll, I can stall. I can see my car. There's no jittering. Client-side prediction stuff works for both me and the NPC. I can do a 1v1 versus an NPC. I can win the game, I can lose the game. The game should last five minutes. Server ticks should stay at like 60 hertz. The client should be at like 240 FPS or more, 99.99% of the time.


## Review (needs-evidence)

### 2026-11-25T23:57:23.918Z — vehicle-ball-contact

Reviewer: vehicle-ball-contact

Handoff continuation 2026-11-25 23:55 UTC. Progress: the free-play profile now applies (vehicle.profile.applied=2 in fresh server run 20261125_233342, profile rl_soccar_freeplay, template octane_rl, gravity 6.5), mode-60 free play launches (mode 60 require_mode_start=false; run 20261125_234424 launched and accepted), and onlinemodes.json ordering is fixed (vehicle_freeplay appended last; mode 50 still maps to vehicle_arena). Jump/air-roll path is now demonstrably live in a fresh server run: 20261125_233342 server-01 journal shows vehicle.jump (delta_vel 2.9167), vehicle.airroll (roll 0.9, yaw +-1, pitch +-0.9), vehicle.jump.double, vehicle.dodge, vehicle.flip, vehicle.jump.apex, and vehicle.state with airborne=true, air_control=true, up_dot_world_up 1.0->-0.59, angular_speed clamped 5.50. The command path vehicle_input local drives these without a physical pad. Remaining top blocker (landing/settle, not air control): run 20261125_233748 shows a 59.6 s continuous stretch (t=107.9-167.6) of airborne=true, grounded_wheels=0, speed<0.5 while the car slowly tumbles on its roof/side, with vehicle.bounce at ~45/s; because ground/flip reset requires all four wheels, no jump reset happens. This directly blocks the staged acceptance items 'wheels rest on surface', 'land on wheels', 'bounce off the world'. Separate evidence-integrity caveat: the scenario runner can pass on retained-server pre-scenario events (see issue-9b5be6c88de0), so scenario verdicts that reuse a server need windowing before they count as proof. Recommended next action: per-wheel contact clamp + chassis-vs-world bounce/restitution, then a landing proof (jump/roll/land on wheels -> settle; land on roof -> bounce, no ground jump; drop from ceiling). Journals: logs/2026-11-25/20261125_233342/, logs/2026-11-25/20261125_233748/, logs/2026-11-25/20261125_234424/.
