// 09 14 2026
/* purpose
* Generic hot melee/contact tool behavior. Keyed by the runtime tool key; reads
* the actor's generic target relationship, checks contact range, and applies
* authoritative damage + a contact effect generically. No NPC-specific melee
* function and no weapon-type switch.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cmath>
#include <cstdint>

namespace {

// NETWORK_WEAPON_SWORDSWORD value (see network/packets.h).
constexpr std::uint64_t kMeleeNetworkId = 4;
constexpr float kMeleeRange = 2.5f;
constexpr std::int32_t kMeleeDamage = 20;

void MIMITA_GAME_CALL meleeUse(const ToolUsePolicyV1* use, GameplayContextV1* ctx)
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

    const float dx = tf.position[0] - use->origin[0];
    const float dy = tf.position[1] - use->origin[1];
    const float dz = tf.position[2] - use->origin[2];
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist > kMeleeRange)
        return;

    auto applyDamage = reinterpret_cast<GameDamageApplyFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_APPLY));
    if (applyDamage) {
        GameDamageApplyV1 request{};
        request.victimEntity = target;
        request.sourceEntity = use->userEntity;
        request.amount = kMeleeDamage;
        request.sourceKind = GAME_DAMAGE_SOURCE_MELEE;
        applyDamage(ctx->host, &request);
    }

    auto spawnEffect = reinterpret_cast<GameEffectSpawnFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_SPAWN));
    if (spawnEffect) {
        GameEffectSpawnV1 fx{};
        fx.kind = gameHash("effect.impact");
        fx.ownerEntity = use->userEntity;
        fx.count = 1;
        fx.position[0] = tf.position[0];
        fx.position[1] = tf.position[1];
        fx.position[2] = tf.position[2];
        fx.scale = 1.0f;
        fx.endScale = 1.0f;
        fx.lifetime = 0.1f;
        spawnEffect(ctx->host, &fx);
    }
}

} // namespace

const MimitaHotPackage::ToolBehaviorRegistrar s_meleeTool{kMeleeNetworkId, meleeUse};

#endif
