// 09 17 2026
/* purpose
* Hot continuous physical-contact tool behavior (family TOOL_BEHAVIOR_CONTACT).
* Godball-like tools are always-active contact shapes: on each accepted use this
* evaluates the generic target relationship and applies contact damage/impulse
* at contact range. Emits generic tool action facts. No Godball/Sword name and
* no weapon-type switch lives here.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-damage-resolve.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-tool-action.h"
#include "hot-reload/hot-tool-tuning.h"
#include "hot-reload/hot-tool-visual.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

constexpr float kContactRange = 2.5f;
constexpr std::int32_t kContactDamage = 20;

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

void MIMITA_GAME_CALL contactUse(const ToolUsePolicyV1* use, GameplayContextV1* ctx)
{
    if (!use || !ctx)
        return;
    auto* mutableUse = const_cast<ToolUsePolicyV1*>(use);
    mutableUse->ammoCost = 0;

    const std::uint64_t key = use->toolId != 0 ? use->toolId : use->toolNetworkId;
    const ToolDefinitionV1* def = findToolDefinition(key);

    GameWeaponTuningV1 tuning{};
    const bool hasTuning = hotQueryWeaponTuning(ctx, use->toolNetworkId, tuning);
    float tuningParamValue = 0.0f;
    const float range = (hasTuning && hotTuningHasParam(tuning, "range", &tuningParamValue))
                            ? tuningParamValue
                            : paramOr(def, "hotRange", kContactRange);
    const float minDamage =
        (hasTuning && tuning.damage > 0.0f)
            ? tuning.damage
            : paramOr(def, "minDamage", (float)kContactDamage);
    const float relativeScale = paramOr(def, "relativeVelocityFactor", 2.0f);

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
    if (dist > range)
        return;

    // We will act: claim the use and suppress the built-in fire.
    mutableUse->outFire = 0;
    mutableUse->handled = 1;

    ToolActionEventV1 accepted{};
    accepted.actorEntity = use->userEntity;
    accepted.toolEntity = use->toolEntity;
    accepted.toolId = key;
    accepted.behaviorId = TOOL_BEHAVIOR_CONTACT;
    accepted.action = TOOL_ACTION_PRIMARY_ACCEPTED;
    accepted.simulationTick = use->tick;
    accepted.origin[0] = use->origin[0];
    accepted.origin[1] = use->origin[1];
    accepted.origin[2] = use->origin[2];
    accepted.direction[0] = use->direction[0];
    accepted.direction[1] = use->direction[1];
    accepted.direction[2] = use->direction[2];
    emitToolAction(ctx, accepted);

    const std::int32_t damage = (std::int32_t)(minDamage +
        (range - dist) * relativeScale);

    // Shared cold consequence owner: damage policy + DamageConfirmed/NPC events.
    GameDamageResolveV1 resolveReq{};
    resolveReq.attackerEntity = use->userEntity;
    resolveReq.weaponEntity = use->toolEntity;
    resolveReq.weaponDefNetworkId = use->toolNetworkId;
    resolveReq.sourceKind = GAME_DAMAGE_SOURCE_CONTACT;
    resolveReq.causeSerial = (std::uint32_t)use->predictionKey;
    resolveReq.victimCount = 1;
    {
        GameDamageVictimV1& victim = resolveReq.victims[0];
        victim.victimEntity = target;
        victim.damage = damage;
        victim.hitPosition[0] = tf.position[0];
        victim.hitPosition[1] = tf.position[1];
        victim.hitPosition[2] = tf.position[2];
        victim.hitNormal[0] = -use->direction[0];
        victim.hitNormal[1] = -use->direction[1];
        victim.hitNormal[2] = -use->direction[2];
    }
    hotResolveDamage(ctx, resolveReq);

    ToolActionEventV1 contact = accepted;
    contact.action = TOOL_ACTION_MELEE_CONTACT;
    contact.amount = damage;
    contact.strength100 = (std::uint32_t)((range - dist) * 100.0f);
    emitToolAction(ctx, contact);
}

} // namespace

const MimitaHotPackage::BehaviorIdRegistrar s_contactBehavior{
    TOOL_BEHAVIOR_CONTACT, contactUse};

#endif
