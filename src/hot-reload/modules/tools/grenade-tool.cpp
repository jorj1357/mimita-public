// 09 17 2026
/* purpose
* Hot grenade-launcher use behavior (family TOOL_BEHAVIOR_GRENADE). Uses the
* canonical hot projectile path: spawns a composition-driven projectile entity
* with bouncy fuse semantics expressed purely as generic component data. Live
* values come from the tool definition params when present. Emits tool actions.
* No new path and no ProjectileType/WeaponType enum.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-prediction.h"
#include "hot-reload/hot-projectile.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/hot-tool-action.h"
#include "hot-reload/hot-tool-state.h"
#include "hot-reload/hot-tool-visual.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// NETWORK_WEAPON_GRENADE_LAUNCHER value (see network/packets.h).
constexpr std::uint64_t kGrenadeNetworkId = 7;

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

void MIMITA_GAME_CALL grenadeUse(const ToolUsePolicyV1* use, GameplayContextV1* ctx)
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
    const float speed = paramOr(def, "hotSpeed", 25.0f);
    const float gravity = paramOr(def, "hotGravity", 22.0f);
    const float fuse = paramOr(def, "hotLifetime", 2.5f);
    const float radius = paramOr(def, "hotRadius", 0.18f);
    const float impactDamage = paramOr(def, "hotImpactDamage", 30.0f);
    const float splashRadius = paramOr(def, "hotSplashRadius", 4.0f);
    const float splashDamage = paramOr(def, "hotSplashDamage", 90.0f);
    const float restitution = paramOr(def, "hotRestitution", 0.45f);

    ToolActionEventV1 accepted{};
    accepted.actorEntity = use->userEntity;
    accepted.toolEntity = use->toolEntity;
    accepted.toolId = key;
    accepted.behaviorId = TOOL_BEHAVIOR_GRENADE;
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
    proj.velocity[2] = dz * speed + 1.0f;
    proj.gravity = gravity;
    proj.lifetime = fuse;
    proj.radius = radius;
    proj.impactDamage = impactDamage;
    proj.splashRadius = splashRadius;
    proj.splashDamage = splashDamage;
    proj.splashExponent = 1.6f;
    proj.knockbackStrength = 10.0f;
    proj.selfDamageMultiplier = 0.2f;
    proj.fullDamageRadius = 1.2f;
    proj.restitution = restitution;
    proj.maxBounces = 3;
    proj.ownerEntity = use->userEntity;
    proj.toolEntity = use->toolEntity;
    proj.typeId = kGrenadeNetworkId;
    proj.flags = HOT_PROJECTILE_BOUNCE_ON_WORLD |
                 HOT_PROJECTILE_EXPLODE_ON_WORLD |
                 HOT_PROJECTILE_EXPLODE_ON_ACTOR |
                 HOT_PROJECTILE_EXPLODE_ON_LIFETIME;
    ctx->dynamicWriteComponent(ctx->host, projectileEntity, HOT_PROJECTILE_COMPONENT,
                               &proj, sizeof(proj));
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
                             use->userEntity, projectileEntity, kGrenadeNetworkId);

    if (use->toolEntity != 0 && ctx->dynamicReadComponent &&
        ctx->dynamicWriteComponent) {
        ToolInstanceStateV1 st = toolStateEnsure(
            ctx, use->toolEntity, key, def ? def->magazineSize : 0,
            def ? def->reserveAmmo : -1, use->userEntity);
        st.cooldownRemaining = def ? def->fireDelay : 0.6f;
        toolStateWrite(ctx, use->toolEntity, st);
    }

    ToolActionEventV1 fired = accepted;
    fired.action = TOOL_ACTION_FIRED;
    fired.direction[0] = dx; fired.direction[1] = dy; fired.direction[2] = dz;
    emitToolAction(ctx, fired);

    char msg[GAME_LOG_MESSAGE];
    std::snprintf(msg, sizeof(msg),
                  "grenade spawned entity=%llu speed=%.1f fuse=%.2f splash=%.1f",
                  (unsigned long long)projectileEntity, speed, fuse, splashDamage);
    toolLogEvent(ctx, 2, "tool.grenade", msg, "fired", projectileEntity,
                 use->userEntity, 1, use->tick);
}

} // namespace

const MimitaHotPackage::BehaviorIdRegistrar s_grenadeBehavior{
    TOOL_BEHAVIOR_GRENADE, grenadeUse};
const MimitaHotPackage::ToolBehaviorRegistrar s_grenadeTool{kGrenadeNetworkId,
                                                            grenadeUse};

#endif
