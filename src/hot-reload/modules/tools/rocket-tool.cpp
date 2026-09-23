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

    if (!ctx->entityCreate || !ctx->dynamicWriteComponent)
        return;

    const std::uint64_t key = use->toolId != 0 ? use->toolId : use->toolNetworkId;
    const ToolDefinitionV1* def = findToolDefinition(key);

    // Prefer the registry tuning (weapons.json / behaviorSource) over literals.
    GameWeaponTuningV1 tuning{};
    const bool hasTuning = hotQueryWeaponTuning(ctx, use->toolNetworkId, tuning);
    auto tparam = [&](const char* name, float fallback) -> float {
        float value = 0.0f;
        if (hasTuning && hotTuningHasParam(tuning, name, &value))
            return value;
        return paramOr(def, name, fallback);
    };
    const float speed = tparam("rocketSpeed",
        (hasTuning && tuning.projectileSpeed > 0.0f) ? tuning.projectileSpeed : 40.0f);
    const float gravity = tparam("gravity", 22.0f);
    const float lifetime = (hasTuning && tuning.projectileLifetime > 0.0f)
                               ? tuning.projectileLifetime
                               : paramOr(def, "hotLifetime", 5.0f);
    const float radius = tparam("rocketRadius",
        (hasTuning && tuning.projectileRadius > 0.0f) ? tuning.projectileRadius : 0.2f);
    const float impactDamage = tparam("rocketDirectDamage", 150.0f);
    const float splashRadius = tparam("splashRadius", 12.0f);
    const float splashDamage = tparam("splashDamage", impactDamage);
    const float splashExponent = tparam("splashExponent", 2.0f);
    const float knockbackStrength = tparam("knockbackStrength", 15.0f);
    const float selfDamageMultiplier = tparam("selfDamageMultiplier", 0.2f);
    const float fullDamageRadius = tparam("full_damage_radius", 1.0f);

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
    proj.flags = HOT_PROJECTILE_EXPLODE_ON_WORLD |
                 HOT_PROJECTILE_EXPLODE_ON_ACTOR |
                 HOT_PROJECTILE_EXPLODE_ON_LIFETIME;
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

    char msg[GAME_LOG_MESSAGE];
    std::snprintf(msg, sizeof(msg),
                  "rocket spawned entity=%llu speed=%.1f dmg=%.0f splash=%.1f",
                  (unsigned long long)projectileEntity, speed, impactDamage,
                  splashDamage);
    toolLogEvent(ctx, 2, "tool.rocket", msg, "fired", projectileEntity,
                 use->userEntity, 1, use->tick);
}

} // namespace

const MimitaHotPackage::BehaviorIdRegistrar s_rocketBehavior{TOOL_BEHAVIOR_ROCKET,
                                                             rocketUse};
const MimitaHotPackage::ToolBehaviorRegistrar s_rocketTool{kRocketNetworkId, rocketUse};

#endif
