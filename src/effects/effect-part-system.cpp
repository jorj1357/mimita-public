// 08 17 2026, 14 20
/* purpose
* Advances pooled effects, blood decals, pending queues, and optional game-DLL simulation.
* Owns fixed-step lifetime cleanup and world-hit queue draining.
* Does NOT spawn weapon hits, render effects, or validate server damage.
*/
#include "effect-part.h"
#include "debug/debug-log.h"
#include "effects/hit-effects.h"
#include "config.h"
#include "replay/replay.h"
#include "perf/perf-spike.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

EffectPartSystem& EffectPartSystem::instance() {
    static EffectPartSystem sInstance;
    return sInstance;
}

void EffectPartSystem::init() {
    for (auto& slot : mPool)
        slot.alive = false;
    mActiveCount = 0;
    mSpawnCursor = 0;
    mFreeSlots.clear();
    mFreeSlots.reserve(POOL_SIZE);
    mActiveSlots.clear();
    mActiveSlots.reserve(POOL_SIZE);
    mActiveSlotPositions.fill(-1);
    for (unsigned int i = 0; i < POOL_SIZE; ++i)
        mFreeSlots.push_back(i);
    mBloodParticles.clear();
    mBloodParticles.reserve(MAX_BLOOD_PARTICLES);
    mSurfaceDecals.clear();
    mSurfaceDecals.reserve(DEFAULT_DECAL_CAP);
    mDecalWriteIdx = 0;
    mPendingBloodDecals.clear();
    mBloodDecalRequests.clear();
    mSpawnFrameCount = 0;
    printf("[EFFECT PART] Initialized pool size=%u\n", POOL_SIZE);
}

void EffectPartSystem::beginFrame() {
    mSpawnFrameCount = 0;
}

void EffectPartSystem::update(float dt) {
    MIMITA_PERF_SCOPE("EffectPartSystem::Update");
    for (size_t activeIndex = 0; activeIndex < mActiveSlots.size();) {
        const unsigned int i = mActiveSlots[activeIndex];
        EffectPart& fx = mPool[i];
        if (!fx.alive) {
            removeActiveSlot(i);
            continue;
        }
        fx.lifetime += dt;
        if (fx.lifetime < 0.0f) {
            ++activeIndex;
            continue;
        }
        if (!fx.sticky) {
            fx.position += fx.velocity * dt;
            if (fx.affectedByGravity)
                fx.velocity.z -= (fx.gravity > 0.0f ? fx.gravity : 9.81f) * dt;
            if (fx.drag > 0.0f)
                fx.velocity *= std::pow(std::max(0.0f, 1.0f - fx.drag), dt * 60.0f);
        }
        if (glm::length(fx.angularVelocity) > 0.0f)
            fx.rotation += fx.angularVelocity * dt;
        if (fx.lifetime >= fx.maxLifetime) {
            if (fx.replayType == "damage_number") {
                Debug::log(Debug::Category::General,
                    "[DAMAGE POPUP] DESTROY label=%s lifetime=%.2f\n",
                    fx.label.c_str(), fx.lifetime);
            } else if (fx.replayType == "damage_impact_sphere") {
                Debug::log(Debug::Category::General,
                    "[DAMAGE IMPACT DESTROY] pos=(%.2f,%.2f,%.2f) lifetime=%.2f alpha=%.2f\n",
                    fx.position.x, fx.position.y, fx.position.z, fx.lifetime, fx.alpha);
            }
            fx.alive = false;
            fx.resetStrings();
            --mActiveCount;
            mFreeSlots.push_back(i);
            removeActiveSlot(i);
            continue;
        }
        ++activeIndex;
    }

    drainPendingWorldHits(6);

    updateBloodParticles(dt);
    updateSurfaceDecals(dt);
    updatePendingBloodDecals(dt);
    processDeferredBloodDecals();
}
