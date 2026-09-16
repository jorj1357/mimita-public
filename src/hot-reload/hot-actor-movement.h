// 09 16 2026
/* purpose
* One shared Source movement tuning table + per-actor movement composition used
* by every hot movement system (local prediction and the server actor system).
* Plain numbers only; no cold structs, no entity-type branches.
* Does NOT own collision, storage, or the network transport.
*/
#pragma once

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"

namespace MimitaHotMovement {

struct MovementMode {
    const char* name;
    float walkSpeed;
    float groundAccel;
    float airAccel;
    float groundFriction;
    float gravity;
    float jumpSpeed;
    float maxFallSpeed;
    float dashImpulse;
    float dashCooldown;
    float downDashSpeed;
    float freeFlySpeed;
};

// One movement tuning authority for all actors. JSON presets are reference.
inline constexpr MovementMode kActorModes[] = {
    // source: fast Source preset (active default)
    {"source", 20.0f, 20.0f, 12.0f, 3.25f, 40.0f, 15.1f, 175.0f, 20.0f, 0.5f, -50.0f, 12.0f},
    // default: instant-control MiMITA preset
    {"default", 20.0f, 55.0f, 22.0f, 1.00f, 58.0f, 18.0f, 400.0f, 100.0f, 0.5f, -100.0f, 12.0f},
};
inline constexpr int kActorModeCount =
    (int)(sizeof(kActorModes) / sizeof(kActorModes[0]));
inline constexpr int kActorSourceModeIndex = 0;

inline const MovementMode& defaultActorMovementMode()
{
    return kActorModes[kActorSourceModeIndex];
}

inline constexpr float kRadPerDegree = 0.01745329252f;

} // namespace MimitaHotMovement
