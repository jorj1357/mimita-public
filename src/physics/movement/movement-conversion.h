// 07 21 2026, 15 45
/* purpose
* Declares conversion boundaries between existing runtime state and shared movement data.
* Keeps Player and server mappings explicit so parity gaps are visible to tests.
* Exposes current runtime movement config without changing existing simulation callers.
* Does NOT implement movement simulation, packet serialization, rendering, or audio.
* Does NOT include server, transport, OpenGL, GLFW, or weapon runtime headers.
* Does NOT invent authority, networking, packet serialization, or simulation ownership.
*/

#pragma once

#include "input/input-frame.h"
#include "input/input-state.h"
#include "physics/movement/movement-types.h"

class Player;

namespace MimitaNet {
struct ServerInput;
struct ServerPlayer;
}

struct MovementServerConversionSupport {
    bool position = true;
    bool baseVelocity = true;
    bool yaw = true;
    bool sizeScale = true;
    bool groundOnGround = true;
    bool dashAvailable = true;
    bool lifecycleSpawnGeneration = true;
    bool lifecycleTransformEpoch = true;

    bool externalImpulse = false;
    bool detailedGroundTimers = false;
    bool jumpTimers = false;
    bool airJumpState = false;
    bool downDashState = false;
    bool freezeTimerState = false;
    bool groundReturnState = false;
    bool dashMomentumProtection = false;
};

MovementCommand movementCommandFromInput(const InputFrame& frame,
                                         const InputState& input,
                                         uint32_t sequence,
                                         uint64_t clientSimulationTick,
                                         MovementLifecycleIdentity lifecycle);

MovementCommand movementCommandFromServerInput(const MimitaNet::ServerInput& input,
                                               uint32_t sequence,
                                               MovementLifecycleIdentity lifecycle);

MovementState movementStateFromPlayer(const Player& player,
                                      MovementLifecycleIdentity lifecycle);

void applyMovementStateToPlayer(const MovementState& state, Player& player);

MovementState movementStateFromServerPlayer(const MimitaNet::ServerPlayer& player);

void applyMovementStateToServerPlayer(const MovementState& state,
                                      MimitaNet::ServerPlayer& player);

MovementConfig makeCurrentRuntimeMovementConfig();

MovementServerConversionSupport currentServerMovementConversionSupport();
