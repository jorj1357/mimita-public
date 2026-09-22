// 09 22 2026
/* purpose
* Implements the per-victim damage policy + event capabilities.
* See server-damage-event.h for scope.
*/
#include "network/server-damage-event.h"

#include <algorithm>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "network/packets.h"
#include "network/server-context.h"
#include "network/server-damage-policy.h"
#include "network/server-gamemode.h"
#include "network/network-weapons.h"
#include "combat/weapon-registry.h"
#include "network/server.h"
#include "ecs/entity-types.h"

namespace MimitaNet {

namespace {

ServerDamageSource toServerDamageSource(std::uint32_t kind)
{
    switch (kind)
    {
    case GAME_DAMAGE_SOURCE_MELEE: return ServerDamageSource::Melee;
    case GAME_DAMAGE_SOURCE_CONTACT: return ServerDamageSource::PhysicalContact;
    case GAME_DAMAGE_SOURCE_EXPLOSION: return ServerDamageSource::RocketExplosion;
    default: return ServerDamageSource::Hitscan;
    }
}

const WeaponDefinition* defForNetworkId(std::uint32_t weaponDefNetworkId)
{
    if (weaponDefNetworkId == 0)
        return nullptr;
    const std::string* id = weaponIdForDefNetworkId(weaponDefNetworkId);
    return id ? WeaponRegistry::instance().get(*id) : nullptr;
}

} // namespace

bool serverDamagePolicyQuery(GameDamagePolicyV1& request)
{
    ServerDamagePolicyInput input{};
    input.source = request.sourceKind;
    input.attackerEntity = request.attackerEntity;
    input.victimEntity = request.victimEntity;
    input.projectileEntity = request.projectileEntity;
    input.weaponNetworkId = request.weaponNetworkId;
    input.victimIsNpc = request.victimIsNpc;
    input.distance = request.distance;
    input.tick = request.tick;
    glm::vec3 knockback(request.knockback[0], request.knockback[1], request.knockback[2]);
    request.outDamage = serverResolveDamagePolicy(input, request.baseDamage, knockback);
    request.knockback[0] = knockback.x;
    request.knockback[1] = knockback.y;
    request.knockback[2] = knockback.z;
    return true;
}

void serverApplyDamageEvent(const GameDamageEventV1& event)
{
    ServerContextV1* context = activeServerContext();
    if (!context || !context->players || !context->npcs || !context->tick)
        return;
    auto& players =
        *static_cast<std::unordered_map<std::uint32_t, ServerPlayer>*>(context->players);
    auto& npcs =
        *static_cast<std::unordered_map<std::uint32_t, ServerNpc>*>(context->npcs);
    const SOCKET sock = static_cast<SOCKET>(context->sock);
    const std::uint32_t tick = static_cast<std::uint32_t>(*context->tick);
    std::uint64_t totalPacketsLocal = 0;
    std::uint64_t& totalPacketsOut =
        context->totalPacketsOut ? *context->totalPacketsOut : totalPacketsLocal;

    const WeaponDefinition* def = defForNetworkId(event.weaponDefNetworkId);
    const std::uint8_t netWeapon =
        def ? networkWeaponTypeForDefinition(*def) : (std::uint8_t)event.weaponNetworkId;
    const ServerDamageSource source = toServerDamageSource(event.sourceKind);
    const glm::vec3 knockback(event.knockback[0], event.knockback[1], event.knockback[2]);
    const glm::vec3 hit(event.hitPosition[0], event.hitPosition[1], event.hitPosition[2]);
    const glm::vec3 normal(event.hitNormal[0], event.hitNormal[1], event.hitNormal[2]);

    const std::uint32_t attackerId = event.attackerEntity != 0
        ? entityLegacyId(static_cast<EntityId>(event.attackerEntity)) : 0;
    auto attackerIt = players.find(attackerId);
    const ServerPlayer* attacker =
        attackerIt != players.end() ? &attackerIt->second : nullptr;

    const std::uint32_t legacy =
        entityLegacyId(static_cast<EntityId>(event.victimEntity));

    auto npcIt = npcs.find(legacy);
    if (npcIt != npcs.end())
    {
        ServerNpc& npc = npcIt->second;
        if (npc.health > 0)
        {
            npc.health -= event.damage;
            npc.knockbackImpulse += knockback;
            if (attacker)
            {
                npc.lastAttackerId = attacker->id;
                npc.lastAttackerPos = attacker->pos;
            }
        }
        const bool killed = npc.health <= 0;
        if (killed)
        {
            npc.health = 0;
            if (attacker && def)
            {
                const char* killWeaponId = networkWeaponTypeName(netWeapon);
                std::string killWeaponDisplay = killWeaponId;
                if (const WeaponDefinition* wd = WeaponRegistry::instance().get(killWeaponId))
                    if (!wd->displayName.empty()) killWeaponDisplay = wd->displayName;
                serverGamemodeRecordKill(sock, players, &npcs,
                    attacker->id, ENTITY_PLAYER,
                    npc.entityId, ENTITY_NPC,
                    killWeaponId, killWeaponDisplay, event.causeSerial,
                    attacker->pos, npc.pos, tick, totalPacketsOut);
            }
        }
        broadcastNpcDamageEvent(sock, players, tick, totalPacketsOut, attackerId,
                                npc, event.damage, killed,
                                attacker ? attacker->pos : hit, hit,
                                glm::normalize(glm::vec3(hit - (attacker ? attacker->pos : hit))),
                                normal, netWeapon);
        return;
    }

    auto targetIt = players.find(legacy);
    if (targetIt == players.end())
        return;
    ServerPlayer& target = targetIt->second;
    if (event.victimSpawnGeneration != 0 &&
        target.spawnGeneration != event.victimSpawnGeneration)
        return;

    ServerDamageResult result = applyServerDamage(
        players, target, attackerId, event.damage, knockback, source);
    queueServerDamageConfirmedEvent(
        sock, players, tick, totalPacketsOut, attackerId, target,
        event.damage, result, hit, normal, knockback,
        source, netWeapon, event.causeSerial, event.projectileId, 0,
        def ? def->id : std::string());
}

} // namespace MimitaNet
