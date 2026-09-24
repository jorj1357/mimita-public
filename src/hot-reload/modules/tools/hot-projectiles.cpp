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
#include "hot-reload/hot-damage-resolve.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-projectile-event.h"
#include "hot-reload/hot-projectile-splash.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/hot-projectile.h"
#include "hot-reload/hot-tool-visual.h"
#include "hot-reload/packages/collision/collision-abi.h"

#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>

namespace {

const std::uint64_t kDomain = gameHash("projectiles.60");

// Network projectile ids (network/packets.h): rocket launcher = 5, grenade
// launcher = 7.
constexpr std::uint64_t kRocketNetworkId = 5;
constexpr std::uint64_t kGrenadeNetworkId = 7;

using DamageApplyFn = bool (MIMITA_GAME_CALL *)(void*, GameDamageApplyV1*);
using EffectSpawnFn = void (MIMITA_GAME_CALL *)(void*, const GameEffectSpawnV1*);

void logProjectileEvent(GameplayContextV1* ctx, const char* name,
                        std::uint64_t tick, const char* fmt, ...)
{
    if (!ctx || !ctx->resolveCapability || !name)
        return;
    auto fn = reinterpret_cast<GameLogEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
    if (!fn)
        return;
    GameLogEventV1 event{};
    event.level = 1u;
    event.simulationTick = static_cast<std::uint32_t>(tick);
    event.serverTick = tick;
    std::snprintf(event.category, sizeof(event.category), "%s", "projectile");
    std::snprintf(event.name, sizeof(event.name), "%s", name);
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(event.message, sizeof(event.message), fmt, args);
        va_end(args);
    }
    fn(nullptr, &event);
}

// True when this process owns a local view (client / listen host). A dedicated
// server has no local player and must not compose screen-only effects.
bool hasLocalPresent(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return false;
    const auto* shared =
        reinterpret_cast<const GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC && shared->localPlayerEntity != 0;
}

// Materialize the projectile's generic presentation if replication did not
// deliver it, so the client always has something to draw.
void ensureProjectilePresentation(GameplayContextV1* ctx, std::uint64_t entity,
                                  std::uint64_t typeId)
{
    if (!ctx->dynamicReadComponent || !ctx->dynamicWriteComponent)
        return;
    HotPresentationStateV1 existing{};
    if (ctx->dynamicReadComponent(ctx->host, entity, HOT_PRESENTATION_COMPONENT,
                                  &existing, sizeof(existing)))
        return;
    const ToolVisualRecipeV1* recipe = findProjectileVisual(typeId);
    if (!recipe || recipe->projectile.meshId == 0)
        return;
    HotPresentationStateV1 present{};
    present.meshResourceId = recipe->projectile.meshId;
    present.textureResourceId = recipe->projectile.textureId;
    present.scale = recipe->projectile.scale > 0.0f ? recipe->projectile.scale : 1.0f;
    for (int k = 0; k < 3; ++k)
        present.color[k] = recipe->projectile.color[k];
    present.color[3] = recipe->projectile.color[3] > 0.0f
                           ? recipe->projectile.color[3] : 1.0f;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_PRESENTATION_COMPONENT,
                               &present, sizeof(present));
}

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
                   const float dir[3], std::uint32_t weaponDefNetworkId,
                   const float hit[3], std::uint64_t tick)
{
    // Route through the shared cold consequence owner (damage policy,
    // DamageConfirmed/NPC-damage events, kill recording) instead of the raw
    // damage.apply primitive.
    GameDamageResolveV1 req{};
    req.attackerEntity = source;
    req.weaponDefNetworkId = weaponDefNetworkId;
    req.sourceKind = GAME_DAMAGE_SOURCE_EXPLOSION;
    req.victimCount = 1;
    GameDamageVictimV1& v = req.victims[0];
    v.victimEntity = victim;
    v.damage = static_cast<std::int32_t>(amount);
    v.knockback[0] = dir ? dir[0] * knockback : 0.0f;
    v.knockback[1] = dir ? dir[1] * knockback : 0.0f;
    v.knockback[2] = dir ? dir[2] * knockback : knockback;
    if (hit)
    {
        v.hitPosition[0] = hit[0];
        v.hitPosition[1] = hit[1];
        v.hitPosition[2] = hit[2];
    }
    GameHealthComponentV1 before{};
    const bool readBefore = ctx->readComponent(
        ctx->host, victim, GAME_COMPONENT_HEALTH, &before, sizeof(before));
    logProjectileEvent(ctx, "projectile.damage.before", tick,
                       "owner=%llu victim=%llu source=%u amount=%.2f health=%d dead=%u",
                       static_cast<unsigned long long>(source),
                       static_cast<unsigned long long>(victim), weaponDefNetworkId,
                       amount, readBefore ? before.current : -1,
                       readBefore ? before.dead : 0u);
    hotResolveDamage(ctx, req);
    GameHealthComponentV1 after{};
    const bool readAfter = ctx->readComponent(
        ctx->host, victim, GAME_COMPONENT_HEALTH, &after, sizeof(after));
    logProjectileEvent(ctx, "projectile.damage.after", tick,
                       "owner=%llu victim=%llu source=%u amount=%.2f health=%d dead=%u",
                       static_cast<unsigned long long>(source),
                       static_cast<unsigned long long>(victim), weaponDefNetworkId,
                       amount, readAfter ? after.current : -1,
                       readAfter ? after.dead : 0u);
    if (readAfter && after.dead && (!readBefore || !before.dead))
        logProjectileEvent(ctx, "actor.death", tick,
                           "actor=%llu attacker=%llu source=%u health_before=%d health_after=%d",
                           static_cast<unsigned long long>(victim),
                           static_cast<unsigned long long>(source), weaponDefNetworkId,
                           readBefore ? before.current : -1, after.current);
}

void explode(GameplayContextV1* ctx, const HotProjectileStateV1& s,
             const float at[3], std::uint64_t projectileEntity,
             std::uint64_t tick)
{
    logProjectileEvent(ctx, "projectile.explosion.before", tick,
                       "projectile=%llu owner=%llu type=%llu pos=(%.3f,%.3f,%.3f) splash=%.3f",
                       static_cast<unsigned long long>(projectileEntity),
                       static_cast<unsigned long long>(s.ownerEntity),
                       static_cast<unsigned long long>(s.typeId), at[0], at[1], at[2],
                       s.splashRadius);
    // Broadcast the authoritative explosion first so remote clients always see
    // it, even if a later local effect path is skipped.
    hotBroadcastProjectileExplode(ctx, (std::uint32_t)projectileEntity, s.ownerEntity,
                                  0u, (std::uint32_t)s.typeId, (std::uint32_t)s.typeId,
                                  at, s.splashRadius);

    // Rocket/grenade detonations compose the shared explosion recipe so the
    // flash/smoke/debris/sound appears where the projectile actually stopped.
    // Only a process with a local view composes it (a dedicated server has no
    // screen and must not author client-only effects). Other projectile types
    // keep the generic impact effect.
    if (hasLocalPresent(ctx) && (s.typeId == kRocketNetworkId ||
                                 s.typeId == kGrenadeNetworkId)) {
        hotComposeExplosion(ctx,
                            s.typeId == kRocketNetworkId
                                ? gameHash("effect.explosion.rocket")
                                : gameHash("effect.explosion.grenade"),
                            at, 1.0f);
    } else {
        spawnImpactEffect(ctx, at);
    }
    if (s.splashRadius <= 0.0f) {
        // Direct-only damage is applied at the contact point below.
        return;
    }
    // Resolve the shared, hot-editable splash falloff policy once for the whole
    // explosion. The falloff curve is hot policy; this system owns the victim
    // enumeration, LOS-free distance test, and damage application.
    const MimitaNet::GameProjectileSplashPolicyV1* splashPolicy = nullptr;
    if (ctx->resolveCapability) {
        auto lookup = reinterpret_cast<MimitaNet::GameProjectileSplashLookupFn>(
            ctx->resolveCapability(ctx->host,
                                   MimitaNet::GAME_CAP_PROJECTILE_SPLASH));
        if (lookup)
            splashPolicy = lookup(ctx->host);
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
        // One policy call per victim; mode 1 preserves the canonical linear
        // power curve exactly. A hot provider may change the curve live.
        MimitaNet::GameSplashFalloffV1 falloffRequest{};
        falloffRequest.structSize = sizeof(MimitaNet::GameSplashFalloffV1);
        falloffRequest.mode = 1u;
        falloffRequest.distance = dist;
        falloffRequest.fullDamageRadius = s.fullDamageRadius;
        falloffRequest.splashRadius = s.splashRadius;
        falloffRequest.splashDamage = s.splashDamage;
        falloffRequest.splashExponent = s.splashExponent;
        if (splashPolicy && splashPolicy->damage)
            splashPolicy->damage(ctx->host, &falloffRequest);
        else
            MimitaNet::HotProjectileSplashImpl::damage(falloffRequest);
        float amount = falloffRequest.outDamage;
        if (actors[i] == s.ownerEntity && s.selfDamageMultiplier > 0.0f)
            amount *= s.selfDamageMultiplier;
        if (amount <= 0.0f)
            continue;
        const float inv = dist > 1e-4f ? 1.0f / dist : 0.0f;
        const float dir[3] = {dx * inv, dy * inv, dz * inv};
        applyDamageTo(ctx, actors[i], s.ownerEntity, amount, s.knockbackStrength,
                      dir, (std::uint32_t)s.typeId, at, tick);
    }
}

void MIMITA_GAME_CALL projectileTick(void* host, std::uint64_t tick, float dt)
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

        bool exploded = false;
        float at[3] = {s.position[0], s.position[1], s.position[2]};

        // Apply a world hit: bounce or explode. Shared by the collision.main
        // owner and the ray fallback so the response is identical.
        auto applyWorldHit = [&](const float hit[3], const float nrm[3]) {
            const bool canBounce =
                (s.flags & HOT_PROJECTILE_BOUNCE_ON_WORLD) &&
                s.bounces < s.maxBounces;
            if (canBounce)
            {
                const float vn = s.velocity[0] * nrm[0] +
                                 s.velocity[1] * nrm[1] +
                                 s.velocity[2] * nrm[2];
                const float k = (1.0f + s.restitution) * vn;
                s.velocity[0] -= k * nrm[0];
                s.velocity[1] -= k * nrm[1];
                s.velocity[2] -= k * nrm[2];
                s.position[0] = hit[0] + nrm[0] * (s.radius + 0.01f);
                s.position[1] = hit[1] + nrm[1] * (s.radius + 0.01f);
                s.position[2] = hit[2] + nrm[2] * (s.radius + 0.01f);
                ++s.bounces;
            }
            else if (s.flags & HOT_PROJECTILE_EXPLODE_ON_WORLD)
            {
                at[0] = hit[0]; at[1] = hit[1]; at[2] = hit[2];
                exploded = true;
            }
        };

        // Universal collision owner first: sweep the projectile sphere through
        // `collision.main` and consume the returned world contact. The ray query
        // is only a fallback when the collision package is unavailable.
        bool worldResolved = false;
        if ((s.flags & HOT_PROJECTILE_EXPLODE_ON_WORLD) && ctx->resolveCapability)
        {
            auto cfn = reinterpret_cast<HotCollisionPackage::GameCollisionSolveFn>(
                ctx->resolveCapability(ctx->host,
                                       HotCollisionPackage::GAME_CAP_COLLISION));
            if (cfn)
            {
                HotCollisionPackage::CollisionSolveV1 q{};
                q.entityId = entities[i];
                q.dt = dt;
                q.mask = HotCollisionPackage::COLLISION_MASK_WORLD;
                for (int k = 0; k < 3; ++k)
                {
                    q.position[k] = prev[k];
                    q.velocity[k] = s.velocity[k];
                }
                q.colliderCount = 1u;
                HotCollisionPackage::CollisionColliderV1& c = q.colliders[0];
                c.partId = HotCollisionPackage::COLLISION_PART_TORSO;
                c.shape = HotCollisionPackage::COLLISION_SHAPE_SPHERE;
                c.policyId = HotCollisionPackage::COLLISION_POLICY_ROCKET;
                c.flags =
                    HotCollisionPackage::COLLISION_COLLIDER_BODY_AUTHORITATIVE;
                c.radius = s.radius;
                for (int k = 0; k < 3; ++k)
                    c.position[k] = prev[k];
                cfn(ctx, &q);
                if (q.handled)
                {
                    worldResolved = true;
                    s.position[0] = q.outPosition[0];
                    s.position[1] = q.outPosition[1];
                    s.position[2] = q.outPosition[2];
                    for (std::uint32_t h = 0; h < q.contactCount; ++h)
                    {
                        if (q.contacts[h].targetKind != 0u)
                            continue;  // world contacts only
                        applyWorldHit(q.contacts[h].point, q.contacts[h].normal);
                        break;
                    }
                }
            }
        }

        // Fallback: integrate and ray-query only when the collision owner is
        // absent (for example a build without the collision package).
        if (!worldResolved)
        {
            s.position[0] += s.velocity[0] * dt;
            s.position[1] += s.velocity[1] * dt;
            s.position[2] += s.velocity[2] * dt;

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
                    if (ctx->queryWorldRay(ctx->host, prev, dir, len + s.radius,
                                           hit, nrm, &dist))
                        applyWorldHit(hit, nrm);
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
                                      2.0f, nullptr, (std::uint32_t)s.typeId,
                                      s.position, tick);
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
            logProjectileEvent(ctx, "projectile.explosion.after", tick,
                               "projectile=%llu owner=%llu type=%llu destroying=1",
                               static_cast<unsigned long long>(entities[i]),
                               static_cast<unsigned long long>(s.ownerEntity),
                               static_cast<unsigned long long>(s.typeId));
            explode(ctx, s, at, entities[i], tick);
            if (s.splashRadius <= 0.0f && s.impactDamage > 0.0f)
                spawnImpactEffect(ctx, at);
            logProjectileEvent(ctx, "projectile.destroy", tick,
                               "projectile=%llu owner=%llu exploded=1",
                               static_cast<unsigned long long>(entities[i]),
                               static_cast<unsigned long long>(s.ownerEntity));
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
        // Projectile visual: recipe-driven, filled in when replication did not
        // carry the presentation component (client-side visibility guarantee).
        ensureProjectilePresentation(ctx, entities[i], s.typeId);
    }
}

} // namespace

const MimitaHotPackage::SchemaRegistrar s_hotProjectileSchema{
    {HOT_PROJECTILE_COMPONENT, gameHash("HotProjectileState.v1"),
     sizeof(HotProjectileStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_ALL,
     "HotProjectileState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_hotProjectileSystem{
    {gameHash("hot.projectile-sim"), kDomain, 0, 0, projectileTick,
     "hot.projectile-sim"}};

#endif
