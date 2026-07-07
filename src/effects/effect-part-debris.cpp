#include "effect-part.h"
#include "world/world.h"
#include "audio/audio.h"
#include "effects/hit-effects.h"
#include "debug/debug-log.h"
#include "config.h"
#include "replay/replay.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>

void EffectPartSystem::spawnWorldDebris(glm::vec3 position, glm::vec3 normal, float force) {
    force = std::max(force, 0.1f);
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0, 0, 1);

    EffectPart e;
    e.position = position + n * 0.04f;
    e.normal = n;
    e.replayType = "debris_batch";
    e.color = {0.42f, 0.40f, 0.38f};
    e.maxLifetime = 0.5f + force * 0.15f + 0.5f;
    e.alpha = 1.0f;
    e.scale = 0.8f + force * 1.0f;
    e.endScale = 4.0f + force * 12.0f;
    e.velocity = {force, 0.0f, 0.0f};
    e.gravity = 15.0f;
    e.affectedByGravity = true;
    e.billboardText = false;
    e.lifetime = 0.0f;
    e.box = false;
    spawn(e);
}
