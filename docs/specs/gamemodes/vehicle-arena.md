# Vehicle Arena

**Purpose:** define the ball, goal, scoring, and match rules for a
Rocket-League-style vehicle arena mode.
**Audience:** gamemode, vehicle, physics, networking, map, HUD, and replay
contributors.
**Status:** desired behavior; reference tuning and first implementation are
pending.
**Does not:** add a second vehicle physics solver, require vehicle-mounted
weapons, or replace the shared gamemode lifecycle.

## 1. Scope

Vehicle Arena is a mode on top of the shared vehicle library. It owns:

- the arena match phases and team rules,
- the ball,
- goal volumes,
- scoring and results,
- arena-specific HUD and reset behavior.

It does not own vehicle suspension, tires, drivetrain, collision, or
replication. Those remain in `docs/specs/vehicles/vehicles.md` and the shared
physical-object owners.

## 2. Core loop

Two teams drive cars in a walled arena. A ball starts at the arena center. Teams
push, hit, or carry the ball toward the opposing goal. A goal scores for the
attacking team, resets the ball, and returns cars to their kickoff positions.
The match ends at the configured score or time limit.

The first playable scope is one arena, one ball, two teams, and no
vehicle-mounted weapons. Existing player-held weapons remain a separate
sandbox/custom-rule option, not a default arena rule.

## 3. Arena and goal data

Arena data belongs to the map and mode configuration, not vehicle code:

- arena bounds and walls use authored map geometry,
- goal volumes are authored zones with a team owner, center, half-extents or
  radius, facing direction, and enabled flag,
- kickoff positions are authored spawn points with a team tag,
- the ball spawn is an authored point or the arena center fallback.

A goal counts when the authoritative ball center crosses the goal plane or
enters the goal volume on the scoring tick. Visual goal geometry may be larger
than the scoring volume; the scoring volume is the authority.

## 4. Ball

- The ball is one dynamic shared `PhysicalEntity` with a stable network id.
- Physics use the shared mass, collision, restitution, friction, and impulse
  path. A sphere may be tessellated for triangle collision; it must not get a
  mode-only collision solver.
- The server owns ball motion, goal detection, and reset.
- Clients receive spawn/despawn/state through the shared physical-entity
  replication path.
- Ball mass, radius, restitution, and drag are mode or map data.
- A goal reset returns the ball to its spawn with zero velocity and no residual
  contact or sleep state.

## 5. Cars in the arena

- Cars use shared vehicle templates and the shared fixed-tick simulation.
- Car mass, suspension, grip, acceleration, and top speed come from templates.
- Boosting, jumping, and any arena-specific movement are mode data on top of
  the shared vehicle input surface; they are not a separate car controller.
- Car contact with the ball is ordinary physical contact. A fast heavy car
  transfers more momentum than a light car without mode-specific branches.
- Car destruction and vehicle-mounted weapons are not required for the first
  playable arena.

## 6. Match lifecycle

The mode uses the shared gamemode lifecycle:

`WAITING -> INTERMISSION -> COUNTDOWN -> GO -> ACTIVE -> RESULTS -> INTERMISSION`

The mode declares:

- team names and capacities,
- score goal,
- time limit and timeout outcome,
- countdown and intermission seconds,
- kickoff layout and spawn assignment,
- overtime or tie behavior when decided,
- whether the match resets the ball and cars on each goal.

A goal during the final active tick still counts if the server accepts it before
the phase transition, matching the shared score-limit ordering rule.

## 7. Scoring and HUD

- The server is the only scoring authority.
- A goal event records tick, scoring team, scorer when known, ball position,
  goal id, and the score before/after.
- Own goals score for the opposing team.
- The HUD shows team scores, active timer, goal flash/reason, and match phase.
- Killfeed-style goal text may use the shared killfeed/presentation owner, but
  scoring data remains mode state.
- Results show the winner, final score, and reason (`score`, `time`, or
  `forfeit`).

## 8. Networking acceptance

- Two or more clients agree on ball position within the existing interpolation
  tolerance.
- A goal is confirmed once by the server; clients do not score locally.
- The same client is never awarded two goals for one crossing.
- Car contact with the ball produces the same visible outcome on all clients.
- A late or duplicate state packet cannot reverse a confirmed goal or score.
- A missing ball or goal definition fails closed with a diagnostic; it does not
  invent a default scoring zone.

## 9. Reference tuning

The first arena tuning should match the current Rocket League gameplay feel for:

- arena dimensions and goal size,
- car acceleration, top speed, boost, jump, and air control,
- ball radius, mass, restitution, drag, and maximum speed,
- kickoff positions and reset timing,
- match length, overtime, and score presentation.

The exact reference values are recorded as a tuning table in this document
before implementation. They are data, not physics branches. Values may be
adjusted after playtests, but the reference table is the starting point.

## 10. Acceptance

1. A car can enter the arena and drive normally using the shared vehicle path.
2. The ball spawns, moves, collides with cars and walls, and resets after a goal.
3. A ball crossing the goal volume scores exactly once for the correct team.
4. Score, timer, goal flash, and results agree across clients.
5. A heavy and a light car produce visibly different ball contact without
   mode-specific physics code.
6. Missing arena data produces a visible diagnostic and no invented goal.
7. The mode does not break when vehicles, the ball, or a client disconnects.

## Related specifications

- [Vehicles](../vehicles/vehicles.md)
- [Gamemodes](gamemodes.md)
- [Objective and round HUD](objective-hud.md)
- [Moving physical objects](../moving-physical-objects/moving-physical-objects.md)
- [Networking](../networking/networking.md)
- [Destructible World](../destructible-world/destructible-world.md)
