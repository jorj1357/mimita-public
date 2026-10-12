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
    // The collision solver is swept, so a normal fixed tick needs one solve.
    // physics-mini.cpp adds only the extra speed-derived substeps needed to
    // keep a fast actor below CollisionConfig::maxSubStepDistance().
    int subSteps = 1,
    const MovementConfig* overrideConfig = nullptr
);
