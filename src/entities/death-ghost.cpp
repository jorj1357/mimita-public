// 08 10 2026, 14 36
/* purpose
* Implements DeathGhostSystem: clones the dying player into a standalone
* fall-over body so the death animation never touches the real player body.
* Renders the clone through the existing Player death-anim render path (frozen
* position + rotation lerp) and removes it when the fall-over completes.
* Does NOT spawn or own network deaths; it only presents the visual.
*/

#include "entities/death-ghost.h"

#include "camera.h"
#include "config/ragdoll-death-config.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "render/render-player.h"

#include <algorithm>
#include <utility>

DeathGhostSystem& DeathGhostSystem::instance()
{
    static DeathGhostSystem system;
    return system;
}

void DeathGhostSystem::spawnFromPlayer(
    const Player& victim,
    const glm::vec3& direction,
    const std::string& actorId,
    uint32_t ownerId)
{
    (void)actorId;
    if (victim.physicalBody.parts.empty() ||
        victim.physicalBody.partMeshes.empty())
        return;

    DeathGhost ghost;
    ghost.ownerId = ownerId;
    ghost.body = victim;  // full copy captures the current pose + meshes
    ghost.body.dead = true;
    ghost.body.netPredictedDead = false;
    ghost.body.currentHp = 0;

    const auto& cfg = RagdollDeathConfig::instance();
    ghost.body.deathAnim.active = true;
    ghost.body.deathAnim.tick = 0;
    ghost.body.deathAnim.totalTicks = cfg.totalTicks();
    ghost.body.deathAnim.startAlpha = cfg.startAlpha();
    ghost.body.deathAnim.endAlpha = cfg.endAlpha();
    ghost.body.deathAnim.startRotation = cfg.startRotation();
    ghost.body.deathAnim.endRotation = cfg.endRotation();
    ghost.body.deathAnim.frozenPosition = victim.pos;

    glm::vec3 dir = glm::length(direction) > 0.001f
        ? glm::normalize(direction)
        : glm::vec3(0.0f, 0.0f, -1.0f);
    const auto& deCfg = HitEffects::config().deathEllipsoid;
    if (deCfg.enabled)
    {
        EffectPartSystem::instance().spawnDeathEllipsoid(
            victim.pos, dir, deCfg.length, deCfg.radius, deCfg.lifetime,
            victim.sizeScale);
    }

    mGhosts.push_back(std::move(ghost));
}

void DeathGhostSystem::update(float dt)
{
    (void)dt;
    for (auto it = mGhosts.begin(); it != mGhosts.end();)
    {
        DeathGhost& ghost = *it;
        if (ghost.body.deathAnim.active)
        {
            ghost.body.deathAnim.tick++;
            if (ghost.body.deathAnim.tick >= ghost.body.deathAnim.totalTicks)
                ghost.body.deathAnim.active = false;
        }
        if (ghost.body.deathAnim.active)
            ++it;
        else
            it = mGhosts.erase(it);
    }
}

void DeathGhostSystem::render(const Camera& camera) const
{
    for (const DeathGhost& ghost : mGhosts)
        renderNetworkPlayer(ghost.body, camera, 0, false);
}

void DeathGhostSystem::removeForOwner(uint32_t ownerId)
{
    if (ownerId == 0)
        return;
    mGhosts.erase(
        std::remove_if(mGhosts.begin(), mGhosts.end(),
            [ownerId](const DeathGhost& ghost) {
                return ghost.ownerId == ownerId;
            }),
        mGhosts.end());
}

void DeathGhostSystem::clear()
{
    mGhosts.clear();
}
