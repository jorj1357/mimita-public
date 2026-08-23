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
#include "hot-reload/hot-reload-system.h"
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
    for (unsigned int i = 0; i < POOL_SIZE; ++i)
        mFreeSlots.push_back(i);
    mBloodParticles.clear();
    mBloodParticles.reserve(MAX_BLOOD_PARTICLES);
    mSurfaceDecals.clear();
    mPendingBloodDecals.clear();
    mBloodDecalRequests.clear();
    mSurfaceDecals.reserve(256);
    printf("[EFFECT PART] Initialized pool size=%u\n", POOL_SIZE);
}

void EffectPartSystem::update(float dt) {
    MIMITA_PERF_SCOPE("EffectPartSystem::Update");
    const GameAPI* gameAPI = HotReloadSystem::instance().gameAPI();
    bool updatedByGameDLL = false;
    if (gameAPI && gameAPI->updateEffects) {
        std::array<GameEffectPartState, POOL_SIZE> states{};
        for (unsigned int i = 0; i < POOL_SIZE; ++i) {
            const EffectPart& effect = mPool[i];
            GameEffectPartState& state = states[i];
            state.position[0] = effect.position.x;
            state.position[1] = effect.position.y;
            state.position[2] = effect.position.z;
            state.velocity[0] = effect.velocity.x;
            state.velocity[1] = effect.velocity.y;
            state.velocity[2] = effect.velocity.z;
            state.lifetime = effect.lifetime;
            state.maxLifetime = effect.maxLifetime;
            state.gravity = effect.gravity;
            state.drag = effect.drag;
            state.alive = effect.alive;
            state.sticky = effect.sticky;
            state.affectedByGravity = effect.affectedByGravity;
        }

        gameAPI->updateEffects(
            &HotReloadSystem::instance().gameMemory(), states.data(), POOL_SIZE, dt);

        for (unsigned int i = 0; i < POOL_SIZE; ++i) {
            EffectPart& effect = mPool[i];
            const GameEffectPartState& state = states[i];
            if (!effect.alive)
                continue;
            effect.position = {state.position[0], state.position[1], state.position[2]};
            effect.velocity = {state.velocity[0], state.velocity[1], state.velocity[2]};
            effect.lifetime = state.lifetime;
            if (!state.alive) {
                effect.alive = false;
                effect.resetStrings();
                --mActiveCount;
                mFreeSlots.push_back(i);
            }
            if (glm::length(effect.angularVelocity) > 0.0f)
                effect.rotation += effect.angularVelocity * dt;
        }
        updatedByGameDLL = true;
    }

    if (!updatedByGameDLL) {
        for (unsigned int i = 0; i < POOL_SIZE; ++i) {
            EffectPart& fx = mPool[i];
            if (!fx.alive)
                continue;
            fx.lifetime += dt;
            if (fx.lifetime < 0.0f)
                continue;
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
            }
        }
    }

    drainPendingWorldHits(6);

    updateBloodParticles(dt);
    updateSurfaceDecals(dt);
    updatePendingBloodDecals(dt);
    processDeferredBloodDecals();
}
