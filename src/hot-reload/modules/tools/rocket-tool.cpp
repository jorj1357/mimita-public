// 09 17 2026
/* purpose
* Hot rocket-launcher use behavior (family TOOL_BEHAVIOR_ROCKET). The hot path is
* the canonical owner: it suppresses the built-in kernel-container spawn and
* spawns a composition-driven projectile entity. Live projectile values come
* from the tool definition params when present. Emits generic tool action facts.
* The canonical projectiles.60 system owns simulation, collision, splash, and
* effects. No kernel WeaponType/ProjectileType branch.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-prediction.h"
#include "hot-reload/hot-projectile-event.h"
#include "hot-reload/hot-projectile.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/hot-tool-action.h"
#include "hot-reload/hot-tool-state.h"
#include "hot-reload/hot-tool-tuning.h"
#include "hot-reload/hot-tool-visual.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// NETWORK_WEAPON_ROCKET_LAUNCHER value (see network/packets.h).
constexpr std::uint64_t kRocketNetworkId = 5;

float paramOr(const ToolDefinitionV1* def, const char* key, float fallback)
{
    if (!def || !def->params)
        return fallback;
    for (std::uint32_t i = 0; i < def->paramCount; ++i) {
        if (def->params[i].key && std::strcmp(def->params[i].key, key) == 0)
            return def->params[i].value;
    }
    return fallback;
}

void MIMITA_GAME_CALL rocketUse(const ToolUsePolicyV1* use, GameplayContextV1* ctx)
{
    if (!use || !ctx)
        return;
    auto* mutableUse = const_cast<ToolUsePolicyV1*>(use);
    // The hot behavior owns the rocket: suppress the built-in spawn.
    mutableUse->outFire = 0;
    mutableUse->ammoCost = 0;
    mutableUse->handled = 1;

    if (!ctx->entityCreate || !ctx->dynamicWriteComponent) {
        // Cannot act: decline so the cold path is not silently swallowed.
        mutableUse->handled = 0;
        return;
    }

    const std::uint64_t key = use->toolId != 0 ? use->toolId : use->toolNetworkId;
    const ToolDefinitionV1* def = findToolDefinition(key);

    {
        char req[GAME_LOG_MESSAGE];
        std::snprintf(req, sizeof(req),
                      "request tool=%llu owner=%llu tick=%llu origin=(%.2f,%.2f,%.2f) "
                      "dir=(%.3f,%.3f,%.3f)",
                      (unsigned long long)key,
                      (unsigned long long)use->userEntity,
                      (unsigned long long)use->tick,
                      use->origin[0], use->origin[1], use->origin[2],
                      use->direction[0], use->direction[1], use->direction[2]);
        toolLogEvent(ctx, 2, "rocket.fire.request", req, "requested",
                     use->toolEntity, use->userEntity, 1, use->tick);
    }

    // Hot automatic-fire policy: the tool's own per-instance state owns the
    // cooldown, so `fire_delay` and fire cadence are hot-editable and no cold
    // launcher has to be consulted. A request while the tool is cooling down is
    // rejected here (one owner), which also makes `fire_mode=automatic` a
    // repeated-tick request naturally rate-limited by fire_delay.
    if (use->toolEntity != 0 && ctx->dynamicReadComponent) {
        ToolInstanceStateV1 existing{};
        if (ctx->dynamicReadComponent(ctx->host, use->toolEntity,
                                      gameHash("ToolInstanceState"), &existing,
                                      sizeof(existing)) &&
            (existing.cooldownRemaining > 0.0f || existing.isReloading)) {
            mutableUse->outFire = 0;
            mutableUse->handled = 1;
            char rejected[GAME_LOG_MESSAGE];
            std::snprintf(rejected, sizeof(rejected),
                          "rejected tool=%llu owner=%llu cooldown=%.3f reloading=%u",
                          (unsigned long long)key,
                          (unsigned long long)use->userEntity,
                          existing.cooldownRemaining, existing.isReloading);
            toolLogEvent(ctx, 2, "rocket.fire.rejected", rejected, "cooldown",
                         use->toolEntity, use->userEntity, 1, use->tick);
            return;
        }
    }

    // Prefer the registry tuning (weapons.json / behaviorSource) over literals.
    GameWeaponTuningV1 tuning{};
    const bool hasTuning = hotQueryWeaponTuning(ctx, use->toolNetworkId, tuning);
    auto tparam = [&](const char* name, float fallback) -> float {
        float value = 0.0f;
        if (hasTuning && hotTuningHasParam(tuning, name, &value))
            return value;
        return paramOr(def, name, fallback);
    };
    // JSON + hot C++ composability: the JSON/tuning value is the data, the
    // multiplier below is the formula. Editing a multiplier here changes the
    // running behavior without a cold rebuild; editing JSON changes the data.
    constexpr float kSpeedMultiplier = 1.0f;
    constexpr float kGravityMultiplier = 1.0f;
    constexpr float kLifetimeMultiplier = 1.0f;
    constexpr float kRadiusMultiplier = 1.0f;
    constexpr float kDamageMultiplier = 1.0f;
    constexpr float kSplashRadiusMultiplier = 1.0f;
    constexpr float kSplashDamageMultiplier = 1.0f;
    constexpr float kKnockbackMultiplier = 1.0f;

    const float speed = tparam("rocketSpeed",
        (hasTuning && tuning.projectileSpeed > 0.0f) ? tuning.projectileSpeed : 40.0f)
        * kSpeedMultiplier;
    const float gravity = tparam("gravity", 22.0f) * kGravityMultiplier;
    const float lifetime = ((hasTuning && tuning.projectileLifetime > 0.0f)
                               ? tuning.projectileLifetime
                               : paramOr(def, "hotLifetime", 5.0f)) * kLifetimeMultiplier;
    const float radius = tparam("rocketRadius",
        (hasTuning && tuning.projectileRadius > 0.0f) ? tuning.projectileRadius : 0.2f)
        * kRadiusMultiplier;
    const float impactDamage = tparam("rocketDirectDamage", 150.0f) * kDamageMultiplier;
    const float splashRadius = tparam("splashRadius", 12.0f) * kSplashRadiusMultiplier;
    const float splashDamage = tparam("splashDamage", impactDamage) * kSplashDamageMultiplier;
    const float splashExponent = tparam("splashExponent", 2.0f);
    const float knockbackStrength = tparam("knockbackStrength", 15.0f) * kKnockbackMultiplier;
    const float selfDamageMultiplier = tparam("selfDamageMultiplier", 0.2f);
    const float fullDamageRadius = tparam("full_damage_radius", 1.0f);
    // World-hit mode: 0 explode, 1 bounce, 2 stop. JSON `worldHitMode`
    // overrides; `maxBounceCount` > 0 implies bounce for legacy configs.
    const float worldHitModeJson = tparam("worldHitMode", -1.0f);
    const float maxBounceCount = tparam("maxBounceCount", 0.0f);
    const std::uint32_t worldHitMode = worldHitModeJson >= 0.0f
        ? (std::uint32_t)worldHitModeJson
        : (maxBounceCount > 0.0f ? 1u : 0u);

    ToolActionEventV1 accepted{};
    accepted.actorEntity = use->userEntity;
    accepted.toolEntity = use->toolEntity;
    accepted.toolId = key;
    accepted.behaviorId = TOOL_BEHAVIOR_ROCKET;
    accepted.action = TOOL_ACTION_PRIMARY_ACCEPTED;
    accepted.simulationTick = use->tick;
    emitToolAction(ctx, accepted);

    float dx = use->direction[0], dy = use->direction[1], dz = use->direction[2];
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 0.001f) { dx = 1.0f; dy = 0.0f; dz = 0.0f; }
    else { dx /= len; dy /= len; dz /= len; }

    std::uint64_t projectileEntity = 0;
    if (!ctx->entityCreate(ctx->host, 0u /* server */, &projectileEntity) ||
        projectileEntity == 0)
        return;

    HotProjectileStateV1 proj{};
    proj.position[0] = use->origin[0];
    proj.position[1] = use->origin[1];
    proj.position[2] = use->origin[2];
    proj.velocity[0] = dx * speed;
    proj.velocity[1] = dy * speed;
    proj.velocity[2] = dz * speed + 2.0f;
    proj.gravity = gravity;
    proj.lifetime = lifetime;
    proj.radius = radius;
    proj.impactDamage = impactDamage;
    proj.splashRadius = splashRadius;
    proj.splashDamage = splashDamage;
    proj.splashExponent = splashExponent;
    proj.knockbackStrength = knockbackStrength;
    proj.selfDamageMultiplier = selfDamageMultiplier;
    proj.fullDamageRadius = fullDamageRadius;
    proj.ownerEntity = use->userEntity;
    proj.toolEntity = use->toolEntity;
    proj.typeId = kRocketNetworkId;
    proj.fireSerial = use->predictionKey;
    proj.weaponNetworkId = use->toolNetworkId != 0 ? use->toolNetworkId : kRocketNetworkId;
    proj.flags = HOT_PROJECTILE_EXPLODE_ON_ACTOR |
                 HOT_PROJECTILE_EXPLODE_ON_LIFETIME;
    // World-hit policy from JSON/hot C++: explode, bounce, or stop.
    switch (worldHitMode) {
    case 1u:  // bounce
        proj.flags |= HOT_PROJECTILE_BOUNCE_ON_WORLD;
        proj.maxBounces = maxBounceCount > 0.0f ? (std::uint32_t)maxBounceCount : 1u;
        proj.restitution = tparam("bounceRestitution", 0.6f);
        break;
    case 2u:  // stop (no world explosion; lifetime still applies)
        break;
    default:  // explode
        proj.flags |= HOT_PROJECTILE_EXPLODE_ON_WORLD;
        break;
    }
    ctx->dynamicWriteComponent(ctx->host, projectileEntity, HOT_PROJECTILE_COMPONENT,
                               &proj, sizeof(proj));

    // Authoritative spawn broadcast so remote clients render the projectile.
    hotBroadcastProjectileSpawn(ctx, (std::uint32_t)projectileEntity, use->userEntity,
                                (std::uint32_t)use->predictionKey,
                                (std::uint32_t)kRocketNetworkId,
                                (std::uint32_t)kRocketNetworkId,
                                proj.position, proj.velocity, proj.radius,
                                proj.lifetime);

    // Generic presentation: logical resource ids only.
    if (ctx->writeComponent) {
        GameTransformComponentV1 tf{};
        tf.position[0] = use->origin[0];
        tf.position[1] = use->origin[1];
        tf.position[2] = use->origin[2];
        tf.look[0] = dx;
        tf.look[1] = dy;
        tf.look[2] = dz;
        ctx->writeComponent(ctx->host, projectileEntity, GAME_COMPONENT_TRANSFORM,
                            &tf, sizeof(tf));
    }
    if (ctx->dynamicWriteComponent) {
        HotPresentationStateV1 present{};
        present.meshResourceId = HOT_MESH_ROCKET;
        present.textureResourceId = HOT_TEX_ROCKET;
        present.scale = 1.0f;
        present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
        ctx->dynamicWriteComponent(ctx->host, projectileEntity,
                                   HOT_PRESENTATION_COMPONENT, &present,
                                   sizeof(present));
    }
    // Generic predicted -> authoritative link.
    if (ctx->dynamicWriteComponent && use->predictionKey != 0) {
        HotPredictionLinkV1 link{};
        link.predictionKey = use->predictionKey;
        ctx->dynamicWriteComponent(ctx->host, projectileEntity,
                                   HOT_PREDICTION_LINK_COMPONENT, &link,
                                   sizeof(link));
    }
    if (ctx->relationshipAdd && use->userEntity != 0)
        ctx->relationshipAdd(ctx->host, gameHash("relationship.fired-projectile"),
                             use->userEntity, projectileEntity, kRocketNetworkId);

    // Per-instance cooldown on the tool entity (no ammo for the rocket here;
    // cold owns reload for now).
    if (use->toolEntity != 0 && ctx->dynamicReadComponent &&
        ctx->dynamicWriteComponent) {
        ToolInstanceStateV1 st = toolStateEnsure(
            ctx, use->toolEntity, key, def ? def->magazineSize : 0,
            def ? def->reserveAmmo : -1, use->userEntity);
        st.cooldownRemaining = def ? def->fireDelay : 0.65f;
        toolStateWrite(ctx, use->toolEntity, st);
    }

    ToolActionEventV1 fired = accepted;
    fired.action = TOOL_ACTION_FIRED;
    fired.amount = def ? def->magazineSize : 0;
    fired.direction[0] = dx; fired.direction[1] = dy; fired.direction[2] = dz;
    emitToolAction(ctx, fired);

    {
        char accepted[GAME_LOG_MESSAGE];
        std::snprintf(accepted, sizeof(accepted),
                      "accepted tool=%llu owner=%llu entity=%llu speed=%.1f lifetime=%.2f",
                      (unsigned long long)key,
                      (unsigned long long)use->userEntity,
                      (unsigned long long)projectileEntity, speed, lifetime);
        toolLogEvent(ctx, 2, "rocket.fire.accepted", accepted, "accepted",
                     projectileEntity, use->userEntity, 1, use->tick);
        char spawnMsg[GAME_LOG_MESSAGE];
        std::snprintf(spawnMsg, sizeof(spawnMsg),
                      "spawn entity=%llu owner=%llu weapon=%llu serial=%llu "
                      "speed=%.1f gravity=%.1f lifetime=%.2f radius=%.2f "
                      "dmg=%.0f splash=%.1f kb=%.1f worldHitMode=%u pos=(%.2f,%.2f,%.2f)",
                      (unsigned long long)projectileEntity,
                      (unsigned long long)use->userEntity,
                      (unsigned long long)proj.weaponNetworkId,
                      (unsigned long long)use->predictionKey, speed, gravity,
                      lifetime, radius, impactDamage, splashDamage,
                      knockbackStrength, worldHitMode, proj.position[0],
                      proj.position[1], proj.position[2]);
        toolLogEvent(ctx, 2, "rocket.spawn", spawnMsg, "spawned",
                     projectileEntity, use->userEntity, 1, use->tick);
        toolLogEvent(ctx, 2, "tool.rocket", spawnMsg, "fired", projectileEntity,
                     use->userEntity, 1, use->tick);
    }
}

} // namespace

const MimitaHotPackage::BehaviorIdRegistrar s_rocketBehavior{TOOL_BEHAVIOR_ROCKET,
                                                             rocketUse};
const MimitaHotPackage::ToolBehaviorRegistrar s_rocketTool{kRocketNetworkId, rocketUse};

#endif
