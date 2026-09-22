// 09 17 2026
/* purpose
* Hot thrown-grenade use behavior (family TOOL_BEHAVIOR_THROWN). Smoke/frag/fire
* are one definition each on the same projectile composition; the type key is
* the tool's own hash so the impact/collision side can distinguish them without
* a kernel enum. Emits generic tool action facts and logs to events.jsonl.
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

void MIMITA_GAME_CALL thrownUse(const ToolUsePolicyV1* use, GameplayContextV1* ctx)
{
    if (!use || !ctx)
        return;
    auto* mutableUse = const_cast<ToolUsePolicyV1*>(use);
    mutableUse->outFire = 0;
    mutableUse->ammoCost = 0;
    mutableUse->handled = 1;

    if (!ctx->entityCreate || !ctx->dynamicWriteComponent)
        return;

    const std::uint64_t key = use->toolId != 0 ? use->toolId : use->toolNetworkId;
    const ToolDefinitionV1* def = findToolDefinition(key);
    GameWeaponTuningV1 tuning{};
    const bool hasTuning = hotQueryWeaponTuning(ctx, use->toolNetworkId, tuning);
    auto tparam = [&](const char* name, float fallback) -> float {
        float value = 0.0f;
        if (hasTuning && hotTuningHasParam(tuning, name, &value))
            return value;
        return paramOr(def, name, fallback);
    };
    const float speed = tparam("throw_speed",
        (hasTuning && tuning.projectileSpeed > 0.0f) ? tuning.projectileSpeed : 18.0f);
    const float gravity = tparam("gravity", 22.0f);
    const float fuse = (hasTuning && tuning.projectileLifetime > 0.0f)
                           ? tuning.projectileLifetime
                           : paramOr(def, "hotLifetime", 3.0f);
    const float radius = (hasTuning && tuning.projectileRadius > 0.0f)
                             ? tuning.projectileRadius
                             : paramOr(def, "hotRadius", 0.12f);
    const float splashRadius = tparam("splashRadius", 4.0f);
    const float splashDamage = tparam("edge_damage",
        tparam("rocketDirectDamage", 40.0f));
    const float restitution = tparam("bounceRestitution", 0.35f);
    const float upBias = tparam("up_bias", 1.0f);
    const std::int32_t magazine =
        hasTuning ? tuning.magazineSize : (def ? def->magazineSize : 1);
    const std::int32_t reserve =
        hasTuning ? tuning.reserveAmmo : (def ? def->reserveAmmo : -1);
    const float fireDelay = hasTuning ? tuning.fireDelay : (def ? def->fireDelay : 1.0f);

    ToolActionEventV1 accepted{};
    accepted.actorEntity = use->userEntity;
    accepted.toolEntity = use->toolEntity;
    accepted.toolId = key;
    accepted.behaviorId = TOOL_BEHAVIOR_THROWN;
    accepted.action = TOOL_ACTION_PRIMARY_ACCEPTED;
    accepted.simulationTick = use->tick;
    emitToolAction(ctx, accepted);

    float dx = use->direction[0], dy = use->direction[1], dz = use->direction[2];
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 0.001f) { dx = 1.0f; dy = 0.0f; dz = 0.0f; }
    else { dx /= len; dy /= len; dz /= len; }

    std::uint64_t projectileEntity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &projectileEntity) ||
        projectileEntity == 0)
        return;

    HotProjectileStateV1 proj{};
    proj.position[0] = use->origin[0];
    proj.position[1] = use->origin[1];
    proj.position[2] = use->origin[2];
    proj.velocity[0] = dx * speed;
    proj.velocity[1] = dy * speed;
    proj.velocity[2] = dz * speed + upBias;
    proj.gravity = gravity;
    proj.lifetime = fuse;
    proj.radius = radius;
    proj.impactDamage = splashDamage;
    proj.splashRadius = splashRadius;
    proj.splashDamage = splashDamage;
    proj.splashExponent = 1.6f;
    proj.knockbackStrength = 8.0f;
    proj.selfDamageMultiplier = 0.2f;
    proj.fullDamageRadius = 1.0f;
    proj.restitution = restitution;
    proj.maxBounces = 3;
    proj.ownerEntity = use->userEntity;
    proj.toolEntity = use->toolEntity;
    proj.typeId = key;   // the tool's own key distinguishes smoke/frag/fire
    proj.flags = HOT_PROJECTILE_BOUNCE_ON_WORLD |
                 HOT_PROJECTILE_EXPLODE_ON_WORLD |
                 HOT_PROJECTILE_EXPLODE_ON_ACTOR |
                 HOT_PROJECTILE_EXPLODE_ON_LIFETIME;
    ctx->dynamicWriteComponent(ctx->host, projectileEntity, HOT_PROJECTILE_COMPONENT,
                               &proj, sizeof(proj));

    // Authoritative spawn broadcast so remote clients render the projectile.
    hotBroadcastProjectileSpawn(ctx, (std::uint32_t)projectileEntity, use->userEntity,
                                (std::uint32_t)use->predictionKey,
                                (std::uint32_t)use->toolNetworkId,
                                (std::uint32_t)use->toolNetworkId,
                                proj.position, proj.velocity, proj.radius,
                                proj.lifetime);
    if (ctx->writeComponent) {
        GameTransformComponentV1 tf{};
        tf.position[0] = use->origin[0];
        tf.position[1] = use->origin[1];
        tf.position[2] = use->origin[2];
        tf.look[0] = dx; tf.look[1] = dy; tf.look[2] = dz;
        ctx->writeComponent(ctx->host, projectileEntity, GAME_COMPONENT_TRANSFORM,
                            &tf, sizeof(tf));
    }
    if (ctx->dynamicWriteComponent) {
        HotPresentationStateV1 present{};
        present.meshResourceId = HOT_MESH_GRENADE;
        present.textureResourceId = HOT_TEX_GRENADE;
        present.scale = 1.0f;
        present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
        ctx->dynamicWriteComponent(ctx->host, projectileEntity,
                                   HOT_PRESENTATION_COMPONENT, &present,
                                   sizeof(present));
    }
    if (ctx->dynamicWriteComponent && use->predictionKey != 0) {
        HotPredictionLinkV1 link{};
        link.predictionKey = use->predictionKey;
        ctx->dynamicWriteComponent(ctx->host, projectileEntity,
                                   HOT_PREDICTION_LINK_COMPONENT, &link,
                                   sizeof(link));
    }
    if (ctx->relationshipAdd && use->userEntity != 0)
        ctx->relationshipAdd(ctx->host, gameHash("relationship.fired-projectile"),
                             use->userEntity, projectileEntity, key);

    if (use->toolEntity != 0 && ctx->dynamicReadComponent &&
        ctx->dynamicWriteComponent) {
        ToolInstanceStateV1 st = toolStateEnsure(
            ctx, use->toolEntity, key, magazine, reserve, use->userEntity);
        st.cooldownRemaining = fireDelay;
        toolStateWrite(ctx, use->toolEntity, st);
    }

    ToolActionEventV1 fired = accepted;
    fired.action = TOOL_ACTION_FIRED;
    fired.direction[0] = dx; fired.direction[1] = dy; fired.direction[2] = dz;
    emitToolAction(ctx, fired);

    char msg[GAME_LOG_MESSAGE];
    std::snprintf(msg, sizeof(msg),
                  "thrown tool=%llu projectile=%llu speed=%.1f fuse=%.2f",
                  (unsigned long long)key,
                  (unsigned long long)projectileEntity, speed, fuse);
    toolLogEvent(ctx, 2, "tool.thrown", msg, "fired", projectileEntity,
                 use->userEntity, 1, use->tick);
}

} // namespace

const MimitaHotPackage::BehaviorIdRegistrar s_thrownBehavior{TOOL_BEHAVIOR_THROWN,
                                                             thrownUse};

#endif
