// 09 14 2026
/* purpose
* Hot rocket-launcher use behavior. The hot path is the canonical owner: it
* suppresses the built-in kernel-container spawn and spawns a composition-driven
* projectile entity (Transform/Velocity + HotProjectileState + ownership
* relationship). The canonical projectiles.60 system owns simulation, collision,
* splash damage, and effects. No kernel WeaponType/ProjectileType branch.
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

// NETWORK_WEAPON_ROCKET_LAUNCHER value (see network/packets.h).
constexpr std::uint64_t kRocketNetworkId = 5;

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
    proj.velocity[0] = dx * 40.0f;
    proj.velocity[1] = dy * 40.0f;
    proj.velocity[2] = dz * 40.0f + 2.0f;
    proj.gravity = 22.0f;
    proj.lifetime = 5.0f;
    proj.radius = 0.2f;
    proj.impactDamage = 120.0f;
    proj.splashRadius = 3.0f;
    proj.splashDamage = 120.0f;
    proj.splashExponent = 2.0f;
    proj.knockbackStrength = 12.0f;
    proj.selfDamageMultiplier = 0.5f;
    proj.fullDamageRadius = 1.0f;
    proj.ownerEntity = use->userEntity;
    proj.typeId = kRocketNetworkId;
    proj.flags = HOT_PROJECTILE_EXPLODE_ON_WORLD |
                 HOT_PROJECTILE_EXPLODE_ON_ACTOR |
                 HOT_PROJECTILE_EXPLODE_ON_LIFETIME;
    ctx->dynamicWriteComponent(ctx->host, projectileEntity, HOT_PROJECTILE_COMPONENT,
                               &proj, sizeof(proj));

    // Generic presentation: logical resource ids only. The canonical hot
    // presentation system draws this entity through render.mesh; no rocket
    // branch exists anywhere in the renderer.
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
    // Generic predicted -> authoritative link: the authoritative entity carries
    // the originating prediction key so the client can retire its provisional.
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

    std::printf("[ROCKET.TOOL] hot projectile entity=%llu owner=%u\n",
                (unsigned long long)projectileEntity, (unsigned)use->ownerId);
}

} // namespace

const MimitaHotPackage::ToolBehaviorRegistrar s_rocketTool{kRocketNetworkId, rocketUse};

#endif
