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

    glm::vec3 tangent = glm::normalize(
        std::abs(n.z) < 0.9f
            ? glm::cross(n, glm::vec3(0, 0, 1))
            : glm::cross(n, glm::vec3(0, 1, 0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));

    const int count = 24 + (int)(force * 8.0f) + rand() % 8;
    const float coneSpread = 0.8f + force * 1.0f;
    const float baseSpeed = 4.0f + force * 12.0f;
    const float lifetimeBase = 0.5f + force * 0.15f;

    for (int i = 0; i < count; ++i) {
        float randomAngle = (float)(rand() % 6283) / 1000.0f;
        float randomRadius = ((float)(rand() % 1001) / 1000.0f) * coneSpread;
        glm::vec3 dir = n
            + tangent * std::cos(randomAngle) * randomRadius
            + bitangent * std::sin(randomAngle) * randomRadius;
        dir = glm::normalize(dir);

        float speed = baseSpeed + (rand() % 5001) / 1000.0f;
        float sx = 0.04f + force * 0.04f + (rand() % 501) / 3000.0f;
        float sy = 0.04f + force * 0.04f + (rand() % 501) / 3000.0f;
        float sz = 0.04f + force * 0.04f + (rand() % 501) / 3000.0f;

        EffectPart e;
        e.position = position + n * 0.04f + dir * (0.02f + (rand() % 51) / 1000.0f);
        e.normal = n;
        e.replayType = "debris_block";
        e.velocity = dir * speed;
        e.angularVelocity = {
            (float)(rand() % 2001 - 1000) / 80.0f * (1.0f + force * 0.5f),
            (float)(rand() % 2001 - 1000) / 80.0f * (1.0f + force * 0.5f),
            (float)(rand() % 2001 - 1000) / 80.0f * (1.0f + force * 0.5f)
        };
        e.color = {0.42f, 0.40f, 0.38f};
        e.maxLifetime = lifetimeBase + (rand() % 501) / 1000.0f;
        e.alpha = 1.0f;
        e.halfSize = {sx, sy, sz};
        e.rotation = {
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360)
        };
        e.billboardText = false;
        e.gravity = 15.0f;
        e.affectedByGravity = true;
        e.lifetime = 0.0f;
        e.box = true;
        spawn(e);
    }
}
