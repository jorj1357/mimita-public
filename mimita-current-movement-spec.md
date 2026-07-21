// 07 20 2026, 18 05
/* purpose
* document current Mimita player movement behavior from source code
* capture local movement, collision, resource resets, and network divergence
* give future movement/network work a concrete current-behavior baseline
* does NOT propose desired movement changes or balance changes
* does NOT replace source code as the authority for exact runtime behavior
* does NOT describe NPC movement except where player collision touches it
*/

# Mimita Current Movement Spec

This file describes current player movement behavior as implemented in the local repository on 07 20 2026. It is a code-grounded baseline, not a design target.

## Primary Source Files

- `src/sim/simulate-tick.cpp`
- `src/physics/physics-mini.cpp`
- `src/physics/config.h`
- `src/physics/movement/physics-walk.cpp`
- `src/physics/movement/physics-jump.cpp`
- `src/physics/movement/physics-dash.cpp`
- `src/physics/movement/physics-down-dash.cpp`
- `src/physics/movement/physics-freeze.cpp`
- `src/physics/movement/physics-ground-return.cpp`
- `src/physics/movement/physics-friction.cpp`
- `src/physics/movement/physics-gravity.cpp`
- `src/physics/movement/physics-collision-*.cpp`
- `src/network/server-players.cpp`
- `src/network/server-packets.cpp`
- `src/network/simulation-constants.h`

## Fixed Timestep

- Local simulation uses `TICK_DT = 1.0f / 60.0f` in `src/sim/simulate-tick.cpp`.
- Shared network constants define `GAMEPLAY_SIMULATION_HZ = 60` and `GAMEPLAY_FIXED_DT = 1.0f / 60.0f` in `src/network/simulation-constants.h`.
- `physicsMainUpdate_Internal(...)` clamps incoming `dt` to at most `0.033f`.
- Server movement uses `SERVER_DT = GAMEPLAY_FIXED_DT`.

## Core Local Constants

From `src/physics/config.h`:

- `AIR_SPEED = 20.0f`
- `PHYS.gravity = -58.0f`
- `PHYS.moveSpeed = 20.0f`
- `PHYS.jumpStrength = 19.0f`
- `PLAYER_WIDTH = 1.0f`
- `PLAYER_HEIGHT = 3.6f`
- `PLAYER_DEPTH = 0.4f`
- `PLAYER_RADIUS = 0.7f`
- `COLLISION_SKIN = 0.02f`
- `MAX_WALKABLE_SLOPE_DOT = 0.80f`
- `ALMOST_ZERO = 0.00001f`
- `MAX_FALL_SPEED = 400.0f`
- `MAX_EXTERNAL_IMPULSE_SPEED = 120.0f`
- `EXTERNAL_IMPULSE_DECAY = 0.6f`
- `COYOTE_JUMP_TIME = 0.001f`
- `GROUND_FRICTION_AMOUNT = 10.0f`
- `AIR_FRICTION_AMOUNT = 2.0f`
- `DASH_IMPULSE = 100.0f`
- `AIR_DASH_IMPULSE = 50.0f`
- `DOWN_DASH_SPEED = -100.0f`
- `GROUND_RETURN_SPEED = -150.0f`
- `FREEZE_MAX_TIME = 5.0f`
- `JUMP_BUFFER_TIME = 0.12f`
- `AIR_JUMPS_MAX = 1`
- `BODY_SAMPLE_RADIUS = 0.15f`
- `MAX_STEP_HEIGHT = 0.25f`

Runtime config currently also matters:

- `config/gameplay.json` has `dash_mode: tf2`.
- `config/collision.json` has `walkableSlopeDot: 0.80`, `bodySampleRadius: 0.25`, `maxStepHeight: 0.25`.
- `config/size_scaling.json` controls size-scaling exponents used by walk, jump, and dash.

## Input Model

`InputState` is the local physics input carrier:

- `wishMoveXY`
- `jumpHeld`
- `jumpPressed`
- `dashPressed`
- `movementPressed`
- `movementJustPressed`
- `groundReturnPressed`
- `downDashPressed`
- `freezeHeld`
- `movementHeldDuration`
- `camForward`

`pollInput(...)` and `buildInputFrame(...)` convert command state into camera-relative movement:

- Forward/back use horizontal camera forward.
- Left/right use `cross(forward, worldUp)`.
- Non-zero movement is normalized.
- Default binds are `W`, `A`, `S`, `D`, `Space`, `Left Shift`, `Q`, `E`, and `R`.

Buffered actions exist for jump, dash, and down-dash in `InputCommandSystem`. `physicsMainUpdate(...)` consumes buffered jump, dash, and down-dash before calling the internal movement function.

## Local Tick Order

`physicsMainUpdate_Internal(...)` currently executes movement in this order:

1. Clamp `dt` to `0.033f`.
2. Store `p.inputWishMove = wishMoveXY`.
3. Clear per-frame flags: ground jump, air jump, dash, down-dash, landing, freeze.
4. Apply gravity.
5. Apply freeze.
6. Apply ground-return.
7. Decay world-contact hysteresis timer.
8. Apply down-dash before collision.
9. Run collision substeps.
10. Copy collision result into `p.ground.onGround`.
11. Update `groundLostTimer` and `stableOnGround`.
12. Track airborne movement-hold ticks for dash quality.
13. Clear external impulse when movement, dash, or freeze input takes control, preserving positive `externalImpulse.z`.
14. Apply walk if movement is held and tick-perfect friction override allows it.
15. Apply air dash if airborne, dash was pressed, and dash is available.
16. Apply jump.
17. Disable tick-perfect friction override after qualifying later input or ability use.
18. If grounded, reset air resources as a safety net.
19. Apply friction.
20. Update airborne timer and landing event state.
21. Update visual facing from camera.
22. Update procedural animation.
23. Optionally apply debug movement.

Important consequence: collision happens before walk, air dash, and jump in the tick. Velocity changes from walk, air dash, and jump affect later ticks, not the collision integration that already ran earlier in the current tick.

## Gravity

`doGravity(...)` adds gravity to vertical velocity every tick:

```cpp
p.vel.z += PHYS.gravity * safeDt;
```

Then it clamps fall speed:

```cpp
if (p.vel.z < -MAX_FALL_SPEED)
    p.vel.z = -MAX_FALL_SPEED;
```

## Walk

`doWalk(...)` is instant velocity assignment, not acceleration:

```cpp
p.vel.x = wishDir.x * maxSpeed;
p.vel.y = wishDir.y * maxSpeed;
```

`maxSpeed` is:

```cpp
(onGround ? PHYS.moveSpeed : AIR_SPEED) * sizeScaleMovementFactor
```

Ground and air speeds are both currently `20.0f` before size scaling. Walk does not accumulate speed. It overwrites base horizontal velocity along the normalized wish direction.

## Friction

`doFriction(...)` uses exponential decay:

```cpp
decay = exp(-frictionAmount * dt)
```

Base velocity XY friction is skipped while grounded and movement input is held. Otherwise:

- grounded friction uses `GROUND_FRICTION_AMOUNT`
- airborne friction uses `AIR_FRICTION_AMOUNT`
- both are multiplied by `p.dash.frictionOverride`
- both are size-scaled with exponent `-0.5f`

External impulse decays independently:

```cpp
impulseDecay = exp(-(EXTERNAL_IMPULSE_DECAY * frictionMul) * dt)
p.externalImpulse *= impulseDecay;
```

External impulse XY is clamped to `MAX_EXTERNAL_IMPULSE_SPEED`.

## Jump

`doJump(...)` manages jump intent, coyote time, ground jump, and air jump.

- `jumpIntentTimer` and `coyoteTimer` decrement by `dt`.
- If `p.ground.onGround`, `coyoteTimer` is reset to `COYOTE_JUMP_TIME`.
- Holding jump refreshes `jumpIntentTimer` to `JUMP_BUFFER_TIME` every frame.
- A new press also sets `jumpIntentTimer = JUMP_BUFFER_TIME`.
- Releasing jump arms air jump and clears jump intent.

Ground jump condition:

```cpp
p.ground.onGround || p.jump.coyoteTimer > 0.0f
```

Ground jump result:

```cpp
p.vel.z = PHYS.jumpStrength * sizeScaleJumpFactor;
p.ground.onGround = false;
p.jump.coyoteTimer = 0.0f;
p.jump.jumpIntentTimer = 0.0f;
p.jump.airJumpsLeft = AIR_JUMPS_MAX;
p.jump.airJumpLocked = true;
p.jump.airJumpArmed = false;
p.jump.didGroundJump = true;
```

Air jump condition:

```cpp
p.jump.airJumpsLeft > 0 && p.jump.airJumpArmed
```

Air jump result sets vertical velocity to the same jump strength, decrements `airJumpsLeft`, disarms air jump, locks it, clears intent, and sets `didAirJump`.

Current air jump count is one via `AIR_JUMPS_MAX = 1`.

## Air Dash

Local `physicsMainUpdate_Internal(...)` calls `doAirDash(...)` only when:

- not grounded
- dash pressed
- `p.dash.dashAvailable`

`doAirDash(...)` additionally requires:

- dash trigger true
- airborne true
- dash available true
- freeze inactive

Dash direction is normalized `wishMoveXY`. If no movement is held, horizontal camera forward is used.

Air dash adds to base velocity XY:

```cpp
p.vel.x += dashDir.x * impulse;
p.vel.y += dashDir.y * impulse;
```

Impulse is:

```cpp
AIR_DASH_IMPULSE * qualityMultiplier * sizeScaleDashFactor
```

`AIR_DASH_IMPULSE` is currently `50.0f`.

Dash consumes dash availability and clears air jumps:

```cpp
p.dash.dashAvailable = false;
p.dash.didDash = true;
p.jump.airJumpsLeft = 0;
```

Tick-perfect dash only applies when gameplay dash mode is `Glide` and `movementHeldDuration < 1.0f / 20.0f`. Current config is `tf2`, so this branch is not active unless config changes.

## Legacy Dash Function

`doDash(...)` still exists but is not called by the current local `physicsMainUpdate_Internal(...)` path. It adds `DASH_IMPULSE` to `p.externalImpulse` and clamps external impulse XY. The active local airborne dash path uses `doAirDash(...)` and modifies `p.vel` directly.

## Down Dash

`doDownDash(...)` runs before collision so collision sees the downward velocity in the same tick.

Conditions:

- down-dash pressed
- freeze inactive
- `p.dash.downDashAvailable`

Result:

```cpp
p.vel.z = DOWN_DASH_SPEED;
p.dash.downDashAvailable = false;
p.dash.didDownDash = true;
```

`DOWN_DASH_SPEED` is currently `-100.0f`.

## Ground Return

`doGroundReturn(...)` is a downward slam.

Conditions:

- ground-return pressed
- `p.groundReturn.available`
- not currently `p.ground.onGround`

Result:

```cpp
p.vel.z = GROUND_RETURN_SPEED;
p.groundReturn.available = false;
```

`GROUND_RETURN_SPEED` is currently `-150.0f`.

Note: `InputCommandSystem::isGroundReturnPressed()` reads action `ground_return`, but default binds shown in `setupDefaultBinds()` do not bind `ground_return`; they bind `down_dash` to `Q` and `freeze` to `E`.

## Freeze

Freeze is edge-triggered by `freezeHeld`.

On press, if available:

```cpp
p.vel = glm::vec3(0.0f);
p.freeze.freezeActive = true;
p.freeze.freezeTimer = 0.0f;
p.freeze.freezeAvailable = false;
p.freeze.didFreeze = true;
```

On release, freeze becomes inactive.

While active:

- timer increases by `dt`
- timer is clamped to `FREEZE_MAX_TIME`
- all components of `p.vel` are multiplied by `freezeVelocityMultiplier(timer)`

Freeze multiplier:

- for `t < 2.5`, `(t / 2.5)^2 * 0.2`
- after that, `0.2 + ((t - 2.5) / 2.5)^2 * 0.8`

Gravity is applied before freeze in the same tick, so freeze scales gravity-influenced velocity after gravity has already modified `p.vel.z`.

## External Impulse

Collision integrates base velocity and external impulse together:

```cpp
(p.vel + p.externalImpulse) * dt
```

When movement input, dash input, or freeze input is active, `physicsMainUpdate_Internal(...)` clears external impulse and preserves only positive upward Z impulse:

```cpp
float upZ = p.externalImpulse.z > 0.0f ? p.externalImpulse.z : 0.0f;
p.externalImpulse = glm::vec3(0.0f);
p.externalImpulse.z = upZ;
```

Jump alone does not clear external impulse.

## Grounding And Contact

Collision owns grounded state.

After collision substeps, `physicsMainUpdate_Internal(...)` assigns:

```cpp
p.ground.onGround = groundedThisFrame;
```

Then stable grounded state is computed as:

```cpp
p.ground.stableOnGround = groundedThisFrame || (p.ground.groundLostTimer < 0.08f);
```

`applyCollisionContact(...)` treats a contact as ground when:

- `normal.z > MAX_WALKABLE_SLOPE_DOT`
- contact point is close to or below feet: `point.z <= feetZ + 0.15f`

On ground contact:

- `groundedThisFrame = true`
- all touch resets are applied
- downward `p.vel.z` is zeroed
- upward `p.externalImpulse.z` is zeroed

Slope, ceiling, and wall contacts do not necessarily set grounded, but they can still reset movement resources.

## Touch Resets

`applyTouchResets(...)` resets resources on surface touch:

```cpp
p.jump.airJumpsLeft = AIR_JUMPS_MAX;
p.jump.airJumpArmed = true;
p.jump.airJumpLocked = false;
p.dash.dashAvailable = true;
p.groundReturn.available = true;
p.dash.downDashAvailable = true;
p.freeze.freezeAvailable = true;
```

This means wall, ceiling, slope, body, weapon, and ground contacts can restore ability resources, depending on the collision path that reports the contact.

## GLB Collision Path

When `world.collisionMesh` is non-empty, `doCollisions(...)` uses `doGLBTriangleCollisions(...)`.

GLB movement starts with:

```cpp
glm::vec3 totalMove = (p.vel + p.externalImpulse) * dt;
```

If the movement is falling faster than one player radius for this collision call, `totalMove.z` is clamped to `-PLAYER_RADIUS`.

Current GLB collision phase order:

1. Body and weapon collision phase.
2. Root capsule sweep and slide.
3. Batched depenetration.
4. Floor recovery.
5. Emergency stuck escape.
6. Stuck position tracking.
7. Debug visualization and trace update.

Body and weapon collision runs before root capsule sweep so arms/weapon/body can push the player away before the root capsule continues into geometry.

Sweep-slide details:

- Up to 5 sweep iterations.
- Step-up can occur when a wall-like normal is hit, `stepHeight > 0`, and `stepHeight <= MAX_STEP_HEIGHT`.
- Step-up requires consistency checks and a walkable floor check after lifting.
- Seam transition can redirect movement against a nearby walkable triangle when a non-walkable seam edge blocks movement near feet.
- Slide projection removes velocity/move components into blocking normals.
- An 8-pass slide-contact projection further removes remaining movement into current slide contacts.

Depenetration uses batched recovery contacts with a slop of `0.01f`, several solver passes, and correction clamps. Emergency escape can zero velocity if a less-penetrating nearby position is found after repeated stuck frames.

## Block Collision Fallback

If no GLB collision mesh exists, `doCollisions(...)` uses the block collision path.

Block collision also integrates:

```cpp
glm::vec3 move = (p.vel + p.externalImpulse) * dt;
```

It performs capsule sweeps against nearby AABBs, attempts step-up for low obstacles, applies batched depenetration, attempts fallback escape, performs ground snap within `0.25f`, and projects velocity against remaining non-ground contacts.

## Player Capsule And Body Collision

The player is not only a movement capsule in the GLB path. The current collision path includes:

- root capsule
- body samples
- weapon capsule/spheres through body-weapon collision

Body/weapon contacts set world-contact timers and call touch resets. Walkable weapon contacts can ground the player.

## Landing State

Landing uses stable-ground transition, not raw collision alone:

```cpp
stableLanding = !prevStableOnGround && p.ground.stableOnGround;
```

Landing event fires only when previous airborne time is greater than `0.08f` and landing cooldown is zero. Landing cooldown is then set to `0.3f`.

## Local Death/Void Flow

`simulateTick(...)` skips physics when `sim.player->dead` is true. It still updates NPCs, death system, spawn flash timer, and void death checks.

## Server Movement Path

Server player movement in `src/network/server-players.cpp` is not the same as local movement.

Server constants:

- `SERVER_TICK_RATE = GAMEPLAY_SIMULATION_HZ`
- `SERVER_DT = GAMEPLAY_FIXED_DT`
- `PLAYER_RADIUS = 0.65f`
- `PLAYER_HEIGHT = 3.5f`

Server fallback movement when no accepted client transform exists:

```cpp
wish = normalized input wish;
target = wish * PHYS.moveSpeed;
horiz += (target - horiz) * min(1.0f, accel * SERVER_DT);
if (wishLen < 0.01f && p.onGround) horiz *= 0.82f;
p.vel.z += PHYS.gravity * SERVER_DT;
if (jumpHeld && onGround) p.vel.z = PHYS.jumpStrength;
if (dashPressed && dashAvailable) p.vel.xy += dashDir * DASH_IMPULSE;
p.pos += p.vel * SERVER_DT;
resolveWorldCollision(p, world);
if (p.onGround) p.dashAvailable = true;
```

Server acceleration values:

- grounded: `55.0f`
- airborne: `22.0f`

Server collision samples three points along the capsule-like vertical body and pushes out from triangles. Server grounded state is set when contact normal `n.z > 0.35f`, which differs from local `MAX_WALKABLE_SLOPE_DOT = 0.80f` ground logic.

## Accepted Client Transform Path

`handleInputPacket(...)` receives client-reported position and velocity:

- `clientPx`, `clientPy`, `clientPz`
- `clientVx`, `clientVy`, `clientVz`

The server validates finite state, component speed, trajectory delta, and authoritative-transform ack state. Movement validation constants in `server.h` are:

- `MAX_HORIZONTAL_SPEED = 150.0f`
- `MAX_UPWARD_SPEED = 80.0f`
- `MAX_DOWNWARD_SPEED = 400.0f`
- `HORIZONTAL_NET_TOL = 4.0f`
- `VERTICAL_NET_TOL = 8.0f`
- `FALL_PREDICTION_TOL = 15.0f`

If accepted:

```cpp
p.pos = reportedPosition;
p.vel = reportedVelocity;
p.clientStateUpdated = true;
```

Then `simulatePlayer(...)` handles `clientStateUpdated` by clearing it, resolving world collision, and returning early:

```cpp
if (p.clientStateUpdated)
{
    p.clientStateUpdated = false;
    resolveWorldCollision(p, world);
    return;
}
```

So accepted client transform ticks do not run the server fallback acceleration, gravity, jump, dash, or position integration. They use client transform plus server collision correction.

## Authoritative Transform Gate

Server spawns, respawns, teleports, joins, rejoins, and reconnects use `beginAuthoritativeTransform(...)`.

That function:

- sets server position and velocity
- increments `transformEpoch`
- marks `awaitingAuthoritativeTransformAck = true`
- stores authoritative position and epoch
- clears `clientStateUpdated`
- resets last accepted client transform tracking to the authoritative transform

While awaiting ack, the server rejects client transforms unless the packet epoch matches and the reported position is within `5.0f` of the authoritative position.

The network client send path in `multiplayer-tick.cpp` also gates outgoing position: if the authoritative server transform has not been locally applied/reconciled, it sends the server position and velocity instead of stale local position and velocity.

## Current Divergences To Preserve In This Spec

- Local walk is instant horizontal velocity assignment; server fallback walk is acceleration toward target velocity.
- Local air dash uses `AIR_DASH_IMPULSE = 50.0f` and writes to `p.vel`; server fallback dash uses `DASH_IMPULSE = 100.0f` and writes to `p.vel`.
- Local ground walkability uses `MAX_WALKABLE_SLOPE_DOT = 0.80f`; server collision marks ground at `n.z > 0.35f`.
- Local collision uses body/weapon collision and detailed GLB sweep/slide/depenetration; server collision uses simpler triangle depenetration samples.
- Local has jump buffering, tiny coyote time, air-jump arming, freeze, down-dash, ground-return, and touch resource resets; server fallback only models basic jump and dash.
- Accepted client transform path bypasses server fallback movement integration and only applies server collision correction.

## Non-Goals

This spec does not say what movement should become. It only records what the current source code does. If networking or prediction work changes movement ownership, this document should be updated after the code changes land.
