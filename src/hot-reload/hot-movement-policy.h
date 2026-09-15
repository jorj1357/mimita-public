// 09 15 2026
/* purpose
* Generic movement-policy payloads shared between the cold movement mechanism
* (which fills inputs and applies the returned velocity) and hot movement
* algorithms (which own how velocity changes). A hot handler that sets `handled`
* owns the actual acceleration/friction/jump/dash math for that step. No
* ServerPlayer/Npc/entity-type fields; inputs are plain numbers so the same hot
* function drives server simulation and local prediction.
* Does NOT own collision/physics or the network transport.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

// Air-acceleration algorithm step. Cold fills the inputs from the shared
// movement state; a hot handler sets `handled` and writes outVelocity[2].
struct GameAirAccelerateV1 {
    float velocity[2];          // in: current horizontal velocity (x,y)
    float wishDir[2];           // in: normalized wish direction
    float wishSpeed;            // in: uncapped wish speed (maxSpeed)
    float wishspd;              // in: Source wish-speed projection cap
    float maxSpeed;             // in: movement max speed
    float airAcceleration;      // in
    float surfaceFriction;      // in
    float airSpeedGainMultiplier;  // in
    float dt;                   // in
    float currentSpeed;         // in: dot(velocity, wishDir)
    float blendedAddSpeed;      // in: addSpeed after blending
    std::uint32_t tick;
    std::uint32_t flags;        // reserved
    // out
    float outVelocity[2];
    std::uint32_t handled;
    std::uint32_t reserved;
};

static constexpr std::uint64_t GAME_EVENT_MOVEMENT_AIR_ACCELERATE =
    gameHash("movement.air-accelerate");

// The single air-acceleration implementation. The `movement.air-accelerate`
// event handler calls it, and local prediction (`movement.main`) calls it
// directly. Context-free: generic/POD numbers only (no ServerPlayer/Player/
// renderer/packet state). Defined once in modules/movement-air.cpp.
namespace MimitaHotMovement {
void airAccelerate(const GameAirAccelerateV1& in, float outVelocity[2]);
}
