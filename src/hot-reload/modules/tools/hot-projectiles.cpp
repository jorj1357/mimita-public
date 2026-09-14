// 09 14 2026
/* purpose
* Canonical hot projectile system. One `projectiles.60` system owns the lifecycle
* of every hot projectile entity (banana, rocket, grenade, future): integration,
* low-level world contact, actor contact, lifetime, splash damage, and effects.
* Uses only generic capabilities (queryWorldRay, findEntities, readComponent,
* damage.apply, effect.spawn, entityDestroy). No projectile-type switch in the
* kernel and no new GameAPI field.
* Does NOT link into the EXE.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-projectile.h"

#include <cmath>
#include <cstdint>

namespace {

const std::uint64_t kDomain = gameHash("projectiles.60");

using DamageApplyFn = bool (MIMITA_GAME_CALL *)(void*, GameDamageApplyV1*);
using EffectSpawnFn = void (MIMITA_GAME_CALL *)(void*, const GameEffectSpawnV1*);

void spawnImpactEffect(GameplayContextV1* ctx, const float pos[3])
{
    if (!ctx->resolveCapability)
        return;
    auto fx = reinterpret_cast<EffectSpawnFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_SPAWN));
    if (!fx)
        return;
    GameEffectSpawnV1 d{};
    d.kind = gameHash("effect.impact");
    d.scale = 1.0f;
    d.lifetime = 0.4f;
    for (int k = 0; k < 3; ++k)
        d.position[k] = pos[k];
    d.color[0] = 1.0f; d.color[1] = 1.0f; d.color[2] = 1.0f; d.color[3] = 1.0f;
    fx(ctx->host, &d);
}

void applyDamageTo(GameplayContextV1* ctx, std::uint64_t victim,
                   std::uint64_t source, float amount, float knockback,
                   const float dir[3])
{
    if (!ctx->resolveCapability)
        return;
    auto dmg = reinterpret_cast<DamageApplyFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_APPLY));
    if (!dmg)
        return;
    GameDamageApplyV1 req{};
    req.victimEntity = victim;
    req.sourceEntity = source;
    req.amount = static_cast<std::int32_t>(amount);
    req.sourceKind = GAME_DAMAGE_SOURCE_EXPLOSION;
    req.knockback[0] = dir ? dir[0] * knockback : 0.0f;
    req.knockback[1] = dir ? dir[1] * knockback : 0.0f;
    req.knockback[2] = dir ? dir[2] * knockback : knockback;
    dmg(ctx->host, &req);
}

void explode(GameplayContextV1* ctx, const HotProjectileStateV1& s,
             const float at[3])
{
    spawnImpactEffect(ctx, at);
    if (s.splashRadius <= 0.0f) {
        // Direct-only damage is applied at the contact point below.
        return;
    }
    std::uint64_t actors[64];
    const std::uint32_t count =
        ctx->findEntities(ctx->host, 0, GAME_COMPONENT_HEALTH, actors, 64);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        GameHealthComponentV1 hp{};
        if (ctx->readComponent(ctx->host, actors[i], GAME_COMPONENT_HEALTH, &hp,
                               sizeof(hp)) && hp.dead)
            continue;
        GameTransformComponentV1 tf{};
        if (!ctx->readComponent(ctx->host, actors[i], GAME_COMPONENT_TRANSFORM, &tf,
                                sizeof(tf)))
            continue;
        const float dx = tf.position[0] - at[0];
        const float dy = tf.position[1] - at[1];
        const float dz = tf.position[2] - at[2];
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > s.splashRadius)
            continue;
        float t = 0.0f;
        if (dist > s.fullDamageRadius && s.splashRadius > s.fullDamageRadius)
            t = (dist - s.fullDamageRadius) / (s.splashRadius - s.fullDamageRadius);
        const float falloff = std::pow(1.0f - t, s.splashExponent > 0.0f ? s.splashExponent : 1.0f);
        float amount = s.splashDamage * falloff;
        if (actors[i] == s.ownerEntity && s.selfDamageMultiplier > 0.0f)
            amount *= s.selfDamageMultiplier;
        if (amount <= 0.0f)
            continue;
        const float inv = dist > 1e-4f ? 1.0f / dist : 0.0f;
        const float dir[3] = {dx * inv, dy * inv, dz * inv};
        applyDamageTo(ctx, actors[i], s.ownerEntity, amount, s.knockbackStrength, dir);
    }
}

void MIMITA_GAME_CALL projectileTick(void* host, std::uint64_t /*tick*/, float dt)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent || !ctx->entityDestroy || !ctx->readComponent ||
        !ctx->findEntities)
        return;

    std::uint64_t entities[128];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_PROJECTILE_COMPONENT, entities, 128);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        HotProjectileStateV1 s{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_PROJECTILE_COMPONENT, &s, sizeof(s)))
            continue;

        s.age += dt;
        s.velocity[2] -= s.gravity * dt;
        if (s.drag > 0.0f) {
            const float f = std::max(0.0f, 1.0f - s.drag * dt);
            s.velocity[0] *= f; s.velocity[1] *= f; s.velocity[2] *= f;
        }
        const float prev[3] = {s.position[0], s.position[1], s.position[2]};
        s.position[0] += s.velocity[0] * dt;
        s.position[1] += s.velocity[1] * dt;
        s.position[2] += s.velocity[2] * dt;

        bool exploded = false;
        float at[3] = {s.position[0], s.position[1], s.position[2]};

        // Low-level world contact.
        if ((s.flags & HOT_PROJECTILE_EXPLODE_ON_WORLD) && ctx->queryWorldRay)
        {
            const float dx = s.position[0] - prev[0];
            const float dy = s.position[1] - prev[1];
            const float dz = s.position[2] - prev[2];
            const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (len > 1e-5f)
            {
                const float dir[3] = {dx / len, dy / len, dz / len};
                float hit[3] = {0.0f, 0.0f, 0.0f};
                float nrm[3] = {0.0f, 0.0f, 1.0f};
                float dist = 0.0f;
                if (ctx->queryWorldRay(ctx->host, prev, dir, len + s.radius, hit,
                                       nrm, &dist))
                {
                    at[0] = hit[0]; at[1] = hit[1]; at[2] = hit[2];
                    exploded = true;
                }
            }
        }

        // Low-level actor contact (direct impact).
        if (!exploded && (s.flags & HOT_PROJECTILE_EXPLODE_ON_ACTOR))
        {
            std::uint64_t actors[64];
            const std::uint32_t actorCount =
                ctx->findEntities(ctx->host, 0, GAME_COMPONENT_HEALTH, actors, 64);
            const float hitRadius = s.radius + 0.6f;
            for (std::uint32_t a = 0; a < actorCount && !exploded; ++a)
            {
                if (actors[a] == s.ownerEntity)
                    continue;
                GameHealthComponentV1 hp{};
                if (ctx->readComponent(ctx->host, actors[a], GAME_COMPONENT_HEALTH,
                                       &hp, sizeof(hp)) && hp.dead)
                    continue;
                GameTransformComponentV1 tf{};
                if (!ctx->readComponent(ctx->host, actors[a],
                                        GAME_COMPONENT_TRANSFORM, &tf, sizeof(tf)))
                    continue;
                const float ddx = tf.position[0] - s.position[0];
                const float ddy = tf.position[1] - s.position[1];
                const float ddz = tf.position[2] - s.position[2];
                if (ddx * ddx + ddy * ddy + ddz * ddz <= hitRadius * hitRadius)
                {
                    if (s.impactDamage > 0.0f)
                        applyDamageTo(ctx, actors[a], s.ownerEntity, s.impactDamage,
                                      2.0f, nullptr);
                    exploded = true;
                }
            }
        }

        // Lifetime expiry.
        if (!exploded && s.age >= s.lifetime)
        {
            if (s.flags & HOT_PROJECTILE_EXPLODE_ON_LIFETIME)
                exploded = true;
            else
            {
                ctx->entityDestroy(ctx->host, entities[i]);
                continue;
            }
        }

        if (exploded)
        {
            explode(ctx, s, at);
            if (s.splashRadius <= 0.0f && s.impactDamage > 0.0f)
                spawnImpactEffect(ctx, at);
            ctx->entityDestroy(ctx->host, entities[i]);
            continue;
        }

        ctx->dynamicWriteComponent(ctx->host, entities[i], HOT_PROJECTILE_COMPONENT,
                                   &s, sizeof(s));
        GameTransformComponentV1 tf{};
        tf.position[0] = s.position[0];
        tf.position[1] = s.position[1];
        tf.position[2] = s.position[2];
        tf.look[0] = 1.0f;
        ctx->writeComponent(ctx->host, entities[i], GAME_COMPONENT_TRANSFORM, &tf,
                            sizeof(tf));
        GameVelocityComponentV1 vl{};
        vl.linear[0] = s.velocity[0];
        vl.linear[1] = s.velocity[1];
        vl.linear[2] = s.velocity[2];
        ctx->writeComponent(ctx->host, entities[i], GAME_COMPONENT_VELOCITY, &vl,
                            sizeof(vl));
    }
}

} // namespace

const MimitaHotPackage::SchemaRegistrar s_hotProjectileSchema{
    {HOT_PROJECTILE_COMPONENT, gameHash("HotProjectileState.v1"),
     sizeof(HotProjectileStateV1), 8, GAME_COPY_RUNTIME_ONLY, 0,
     "HotProjectileState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_hotProjectileSystem{
    {gameHash("hot.projectile-sim"), kDomain, 0, 0, projectileTick,
     "hot.projectile-sim"}};

#endif
