// 07 21 2026, 16 30
/* purpose
* Declares the public local physics update entry point.
* Keeps callers independent from individual movement wrapper headers.
* Lets physics-mini.cpp orchestrate shared movement and collision internally.
* Does NOT declare subsystem-owned dash, jump, friction, freeze, or collision helpers.
* Does NOT expose legacy movement formulas or stale compatibility overloads.
* Does NOT include rendering, networking, packet, audio, or weapon APIs.
*/

#pragma once

#include "physics/movement/movement-types.h"

struct Player;
struct World;
struct InputState;

// `overrideConfig` (optional) supplies a MovementConfig that replaces the
// global player movement preset for this update. Used by NPCs so they can run
// a different movement preset than the player. nullptr = global config.
void physicsMainUpdate(
    Player& player,
    const World& world,
    const InputState& input,
    float dt,
    int subSteps = 6,
    const MovementConfig* overrideConfig = nullptr
);
