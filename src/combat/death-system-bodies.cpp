#include "combat/death-system.h"

#include <algorithm>
#include <cmath>

#include <glad/glad.h>
#include "config.h"
#include "debug/debug-log.h"
#include "camera.h"
#include "physics/config.h"
#include "world/world.h"
#include "renderer/renderer.h"
#include "replay/replay.h"

extern Renderer* gRenderer;

void DeathSystem::render(const Camera&) const
{
    // Death animation is rendered through the normal player rendering pipeline.
    // The dead player model is drawn with alpha and rotation overrides applied
    // in Player::updateModelWorldTransforms() and Player::renderCurrentPose().
    // No separate corpse rendering needed.
}

void DeathSystem::appendReplayActors(std::vector<ReplayActorState>& actors) const
{
    // Dead players already appear as regular actors in the scene frame
    // via the replay capture in engine-tick-replay.cpp.
    // Their dead state is captured through player.dead and deathAnim fields.
    // No separate corpse actors are needed.
}
