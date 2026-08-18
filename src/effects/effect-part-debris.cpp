// 08 17 2026, 14 20
/* purpose
* Creates persistent world-debris batches from impact points and surface normals.
* Owns debris spawn velocity, lifetime, gravity, and size scaling inputs.
* Does NOT render chunks, own collision authority, or decide when impacts occur.
*/
#include "effect-part.h"
#include "config/size-scaling-config.h"
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

void EffectPartSystem::spawnWorldDebris(glm::vec3 position, glm::vec3 normal, float force, float sizeScale) {
    force = std::max(force, 0.1f);
    const auto& cfg = HitEffects::config().worldDebris;
    if (!cfg.enabled) return;
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0, 0, 1);
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float sfx = sc.scale(1.0f, sc.debrisSizeExponent, ss);
    float cfx = sc.scale(1.0f, sc.debrisCountExponent, ss);

    EffectPart e;
    e.position = position + n * cfg.surfaceOffset * sfx;
    e.normal = n;
    e.replayType = "debris_batch";
    e.color = cfg.color;
    e.maxLifetime = (cfg.lifetimeBase + force * cfg.lifetimeForce + cfg.lifetimeBase) * cfx;
    e.alpha = 1.0f;
    e.scale = (cfg.startScale + force * cfg.scaleForce) * sfx;
    e.endScale = (cfg.endScale + force * cfg.endScaleForce) * sfx;
    e.velocity = n * (force * cfg.speed * cfx);
    e.gravity = cfg.gravity;
    e.affectedByGravity = true;
    e.billboardText = false;
    e.lifetime = 0.0f;
    e.box = false;
    e.debrisCount = std::max(0, cfg.count);
    spawn(e);
}
