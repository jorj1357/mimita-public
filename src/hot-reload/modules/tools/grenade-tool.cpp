// 09 14 2026
/* purpose
* Hot grenade-launcher use behavior. Uses the canonical hot projectile path:
* spawns a composition-driven projectile entity with bouncy fuse semantics
* expressed purely as generic component data (bounce flag, restitution, fuse
* lifetime, splash). No new path and no ProjectileType/WeaponType enum.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-prediction.h"
#include "hot-reload/hot-projectile.h"
#include "hot-reload/hot-presentation.h"

#include <cmath>
#include <cstdio>

namespace {

// NETWORK_WEAPON_GRENADE_LAUNCHER value (see network/packets.h).
constexpr std::uint64_t kGrenadeNetworkId = 7;

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
    proj.velocity[0] = dx * 25.0f;
    proj.velocity[1] = dy * 25.0f;
    proj.velocity[2] = dz * 25.0f + 1.0f;
    proj.gravity = 22.0f;
    proj.lifetime = 2.5f;      // fuse
    proj.radius = 0.18f;
    proj.impactDamage = 30.0f;
    proj.splashRadius = 4.0f;
    proj.splashDamage = 90.0f;
    proj.splashExponent = 1.6f;
    proj.knockbackStrength = 10.0f;
    proj.selfDamageMultiplier = 0.5f;
    proj.fullDamageRadius = 1.2f;
    proj.restitution = 0.45f;
    proj.maxBounces = 3;
    proj.ownerEntity = use->userEntity;
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

    std::printf("[GRENADE.TOOL] hot projectile entity=%llu owner=%u\n",
                (unsigned long long)projectileEntity, (unsigned)use->ownerId);
}

} // namespace

const MimitaHotPackage::ToolBehaviorRegistrar s_grenadeTool{kGrenadeNetworkId,
                                                            grenadeUse};

#endif
