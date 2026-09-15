// 09 14 2026
/* purpose
* Hot authoritative NPC/monster combat behavior on gameplay.60. Reads generic
* actor state (ActorHealthState/ActorTeamState/Transform), chooses a target,
* writes the relationship.targets edge, and performs a real authoritative
* attack through the generic damage.apply capability. No MonsterType/NpcType,
* no NPC-specific capability, no cold weapon branch.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cmath>
#include <cstdint>

namespace {

const std::uint64_t kHealthState = gameHash("ActorHealthState");
const std::uint64_t kTeamState = gameHash("ActorTeamState");
const std::uint64_t kTargetsRel = gameHash("relationship.targets");
const std::uint64_t kCooldownState = gameHash("NpcAttackCooldown");
const std::uint64_t kEquipsItemRel = gameHash("relationship.equips-item");
const std::uint64_t kToolRefState = gameHash("ToolRefState");

struct ToolRefStateV1 {
    std::uint64_t toolKey;
    std::uint64_t reserved;
};

struct ActorHealthStateV1 {
    std::int32_t current;
    std::int32_t max;
    std::uint32_t dead;
    std::uint32_t reserved;
};
struct ActorTeamStateV1 {
    std::int32_t team;
    std::uint32_t reserved;
};
struct NpcAttackCooldownV1 {
    std::uint64_t lastAttackTick;
    std::uint32_t attacks;
    std::uint32_t reserved;
};

constexpr std::int32_t kAttackDamage = 10;
constexpr float kAttackRange = 2.5f;
constexpr float kAcquireRange = 40.0f;
constexpr std::uint64_t kAttackCooldownTicks = 60;

bool readTransform(GameplayContextV1* ctx, std::uint64_t entity, float out[3])
{
    GameTransformComponentV1 tf{};
    if (!ctx->readComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                            sizeof(tf)))
        return false;
    out[0] = tf.position[0];
    out[1] = tf.position[1];
    out[2] = tf.position[2];
    return true;
}

void MIMITA_GAME_CALL npcCombatTick(void* host, std::uint64_t tick, float /*dt*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent || !ctx->readComponent || !ctx->relationshipAdd ||
        !ctx->relationshipQuery || !ctx->resolveCapability)
        return;

    std::uint64_t actors[96] = {0};
    const std::uint32_t count =
        ctx->dynamicEnumerateComponent(ctx->host, kHealthState, actors, 96);

    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t self = actors[i];
        ActorHealthStateV1 selfHealth{};
        if (!ctx->dynamicReadComponent(ctx->host, self, kHealthState, &selfHealth,
                                       sizeof(selfHealth)))
            continue;
        if (selfHealth.current <= 0)
            continue;

        float selfPos[3] = {0.0f, 0.0f, 0.0f};
        if (!readTransform(ctx, self, selfPos))
            continue;

        ActorTeamStateV1 selfTeam{};
        ctx->dynamicReadComponent(ctx->host, self, kTeamState, &selfTeam, sizeof(selfTeam));

        // Choose the nearest eligible target with a different team.
        std::uint64_t best = 0;
        float bestDist = kAcquireRange;
        for (std::uint32_t j = 0; j < count; ++j) {
            const std::uint64_t other = actors[j];
            if (other == self)
                continue;
            ActorHealthStateV1 otherHealth{};
            if (!ctx->dynamicReadComponent(ctx->host, other, kHealthState, &otherHealth,
                                           sizeof(otherHealth)))
                continue;
            if (otherHealth.current <= 0)
                continue;
            ActorTeamStateV1 otherTeam{};
            ctx->dynamicReadComponent(ctx->host, other, kTeamState, &otherTeam,
                                      sizeof(otherTeam));
            if (selfTeam.team >= 0 && otherTeam.team == selfTeam.team)
                continue;  // same team
            float otherPos[3] = {0.0f, 0.0f, 0.0f};
            if (!readTransform(ctx, other, otherPos))
                continue;
            const float dx = otherPos[0] - selfPos[0];
            const float dy = otherPos[1] - selfPos[1];
            const float dz = otherPos[2] - selfPos[2];
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < bestDist) {
                bestDist = dist;
                best = other;
            }
        }

        // Publish target as generic entity-to-entity state.
        std::uint64_t current[1] = {0};
        const std::uint32_t have =
            ctx->relationshipQuery(ctx->host, kTargetsRel, self, current, nullptr, 1);
        if (have == 0 || current[0] != best) {
            if (have > 0)
                ctx->relationshipRemove(ctx->host, kTargetsRel, self, current[0]);
            if (best != 0)
                ctx->relationshipAdd(ctx->host, kTargetsRel, self, best, 0);
        }

        if (best == 0 || bestDist > kAttackRange)
            continue;

        // Resolve the equipped tool entity through the generic equip
        // relationship; the tool carries the runtime key in ToolRefState.
        std::uint64_t toolEntity = 0;
        std::uint64_t toolKey = 0;
        {
            std::uint64_t equipped[1] = {0};
            if (ctx->relationshipQuery &&
                ctx->relationshipQuery(ctx->host, kEquipsItemRel, self, equipped,
                                       nullptr, 1) > 0) {
                toolEntity = equipped[0];
                ToolRefStateV1 ref{};
                if (ctx->dynamicReadComponent &&
                    ctx->dynamicReadComponent(ctx->host, toolEntity, kToolRefState,
                                              &ref, sizeof(ref)))
                    toolKey = ref.toolKey;
            }
        }

        // Fire rate state lives on the tool entity (the tool decides), falling
        // back to the actor for tool-less monsters.
        const std::uint64_t cooldownOwner = toolEntity != 0 ? toolEntity : self;
        NpcAttackCooldownV1 cooldown{};
        const bool hasCooldown = ctx->dynamicReadComponent(
            ctx->host, cooldownOwner, kCooldownState, &cooldown, sizeof(cooldown));
        if (hasCooldown && tick - cooldown.lastAttackTick < kAttackCooldownTicks)
            continue;
        cooldown.lastAttackTick = tick;
        cooldown.attacks += 1;
        ctx->dynamicWriteComponent(ctx->host, cooldownOwner, kCooldownState, &cooldown,
                                   sizeof(cooldown));

        if (toolEntity != 0 && toolKey != 0 && ctx->emitEvent) {
            // Generic action: the tool behavior owns the weapon semantics.
            float bestPos[3] = {0.0f, 0.0f, 0.0f};
            readTransform(ctx, best, bestPos);
            float dx = bestPos[0] - selfPos[0];
            float dy = bestPos[1] - selfPos[1];
            float dz = bestPos[2] - selfPos[2];
            const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (len < 0.001f) { dx = 1.0f; dy = 0.0f; dz = 0.0f; }
            else { dx /= len; dy /= len; dz /= len; }

            ToolUsePolicyV1 use{};
            use.userEntity = self;
            use.toolEntity = toolEntity;
            use.toolId = toolKey;
            use.toolNetworkId = static_cast<std::uint32_t>(toolKey);
            use.kind = 0;
            use.tick = static_cast<std::uint32_t>(tick);
            use.baseFire = 1;
            use.outFire = 1;
            use.origin[0] = selfPos[0]; use.origin[1] = selfPos[1]; use.origin[2] = selfPos[2];
            use.direction[0] = dx; use.direction[1] = dy; use.direction[2] = dz;

            GameEventV1 event{};
            event.typeId = gameHash("tool.primary-use");
            event.schemaHash = gameHash("tool.use.v1");
            event.payloadVersion = 1;
            event.payloadSize = sizeof(use);
            event.tick = tick;
            event.sourceEntity = self;
            event.targetEntity = best;
            event.payload = &use;
            reinterpret_cast<GameEmitEventFn>(ctx->emitEvent)(ctx, &event);
        } else {
            // Tool-less monster: direct authoritative damage fallback.
            auto applyDamage = reinterpret_cast<GameDamageApplyFn>(
                ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_APPLY));
            if (applyDamage) {
                GameDamageApplyV1 request{};
                request.victimEntity = best;
                request.sourceEntity = self;
                request.amount = kAttackDamage;
                request.sourceKind = GAME_DAMAGE_SOURCE_MELEE;
                applyDamage(ctx->host, &request);
            }
        }
    }
}

} // namespace

const MimitaHotPackage::SystemRegistrar s_npcCombat{
    {gameHash("npc.combat-ai"), GAME_DOMAIN_GAMEPLAY, 650, 0, npcCombatTick,
     "npc.combat-ai"}};
const MimitaHotPackage::SchemaRegistrar s_npcAttackCooldownSchema{
    {kCooldownState, gameHash("NpcAttackCooldown.v1"), sizeof(NpcAttackCooldownV1), 8,
     GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE, "NpcAttackCooldown", 1, 0}};

#endif
