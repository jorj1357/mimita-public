// 09 14 2026
/* purpose
* Generic hot hitscan tool behavior. Keyed by the runtime tool key; reads the
* actor's generic target relationship, raycasts the world through the kernel
* query primitive, and applies authoritative damage + an effect generically. It
* owns range/damage/occlusion policy. No NPC-specific hitscan function and no
* weapon-type switch.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cmath>
#include <cstdint>

namespace {

// NETWORK_WEAPON_REVOLVER value (see network/packets.h).
constexpr std::uint64_t kRevolverNetworkId = 1;
constexpr float kHitscanRange = 40.0f;
constexpr std::int32_t kHitscanDamage = 25;

void MIMITA_GAME_CALL hitscanUse(const ToolUsePolicyV1* use, GameplayContextV1* ctx)
{
    if (!use || !ctx)
        return;
    auto* mutableUse = const_cast<ToolUsePolicyV1*>(use);
    mutableUse->outFire = 0;
    mutableUse->handled = 1;

    if (!ctx->relationshipQuery || !ctx->readComponent || !ctx->resolveCapability)
        return;

    std::uint64_t target = 0;
    if (ctx->relationshipQuery(ctx->host, gameHash("relationship.targets"),
                               use->userEntity, &target, nullptr, 1) == 0 ||
        target == 0)
        return;

    GameTransformComponentV1 tf{};
    if (!ctx->readComponent(ctx->host, target, GAME_COMPONENT_TRANSFORM, &tf,
                            sizeof(tf)))
        return;

    const float ox = use->origin[0], oy = use->origin[1], oz = use->origin[2];
    float dx = tf.position[0] - ox;
    float dy = tf.position[1] - oy;
    float dz = tf.position[2] - oz;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist > kHitscanRange || dist < 0.001f)
        return;
    dx /= dist; dy /= dist; dz /= dist;

    // Low-level occlusion query stays a kernel primitive; the behavior owns the
    // policy of whether a blocked shot hits.
    float point[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
    float wallDist = 0.0f;
    if (ctx->queryWorldRay &&
        ctx->queryWorldRay(ctx->host, use->origin, use->direction, kHitscanRange,
                           point, normal, &wallDist)) {
        if (wallDist < dist - 0.5f)
            return;  // world blocks the shot
    }

    auto applyDamage = reinterpret_cast<GameDamageApplyFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_APPLY));
    if (applyDamage) {
        GameDamageApplyV1 request{};
        request.victimEntity = target;
        request.sourceEntity = use->userEntity;
        request.amount = kHitscanDamage;
        request.sourceKind = GAME_DAMAGE_SOURCE_HITSCAN;
        applyDamage(ctx->host, &request);
    }

    auto spawnEffect = reinterpret_cast<GameEffectSpawnFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_SPAWN));
    if (spawnEffect) {
        GameEffectSpawnV1 fx{};
        fx.kind = gameHash("effect.muzzle");
        fx.ownerEntity = use->userEntity;
        fx.count = 1;
        fx.position[0] = use->origin[0];
        fx.position[1] = use->origin[1];
        fx.position[2] = use->origin[2];
        fx.direction[0] = dx; fx.direction[1] = dy; fx.direction[2] = dz;
        fx.scale = 1.0f;
        fx.endScale = 1.0f;
        fx.speed = 0.0f;
        fx.lifetime = 0.08f;
        spawnEffect(ctx->host, &fx);
    }
}

} // namespace

const MimitaHotPackage::ToolBehaviorRegistrar s_hitscanTool{kRevolverNetworkId,
                                                            hitscanUse};

#endif
