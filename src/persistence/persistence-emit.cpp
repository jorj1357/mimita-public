// 09 01 2026, 00 00
/* purpose
* Implement server-side persistence event emission helpers.
* Build PersistenceKillEvent structs from authoritative server data
* and enqueue them to the async PersistenceQueue.
* Does NOT block the simulation tick or wait for backend confirmation.
*/

#include "persistence/persistence-emit.h"
#include "persistence/persistence-queue.h"
#include "persistence/persistence-events.h"
#include "network/server.h"
#include "auth/auth-system.h"

#include <cstdio>
#include <sstream>
#include <iomanip>

namespace MimitaNet {

static std::string makeEventId(const char* prefix, uint32_t tick,
                               uint64_t attacker, uint64_t victim)
{
    std::ostringstream ss;
    ss << prefix << "_" << tick << "_" << attacker << "_" << victim;
    return ss.str();
}

static int64_t resolveAccountId(const ServerPlayer& p)
{
    return p.accountId > 0 ? static_cast<int64_t>(p.accountId) : 0;
}

static float distanceBetween(const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 d = b - a;
    return glm::length(d);
}

static const char* weaponIdToName(uint8_t weapon)
{
    switch (weapon)
    {
    case 1: return "revolver";
    case 2: return "godball";
    case 3: return "shotgun";
    case 4: return "swordsword";
    case 5: return "rocket_launcher";
    case 6: return "hafs";
    case 7: return "grenade_launcher";
    case 8: return "aa12";
    case 9: return "spyknife";
    default: return "unknown";
    }
}

void emitPvPKillPersistenceEvent(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t attackerPlayerId,
    uint32_t victimPlayerId,
    uint32_t weaponId,
    uint32_t serverTick,
    const glm::vec3& attackerPos,
    const glm::vec3& victimPos)
{
    emitPvPKillPersistenceEvent(players, attackerPlayerId, victimPlayerId,
        std::string(weaponIdToName(weaponId)), serverTick, attackerPos, victimPos);
}

void emitPvPKillPersistenceEvent(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t attackerPlayerId,
    uint32_t victimPlayerId,
    const std::string& weaponName,
    uint32_t serverTick,
    const glm::vec3& attackerPos,
    const glm::vec3& victimPos)
{
    auto attIt = players.find(attackerPlayerId);
    auto vitIt = players.find(victimPlayerId);
    if (attIt == players.end() || vitIt == players.end())
        return;

    PersistenceKillEvent event;
    event.eventId = makeEventId("kill", serverTick, attackerPlayerId, victimPlayerId);
    event.serverTick = serverTick;
    event.attackerType = "player";
    event.attackerId = resolveAccountId(attIt->second);
    event.attackerName = attIt->second.name;
    event.victimType = "player";
    event.victimId = resolveAccountId(vitIt->second);
    event.victimName = vitIt->second.name;
    event.weaponId = weaponName;
    event.distanceMeters = distanceBetween(attackerPos, victimPos);

    PersistenceQueue::instance().enqueueKill(event);

    printf("[PERSISTENCE] PvP kill emitted: %s killed %s weapon=%s dist=%.1f accountId=%lld/%lld\n",
           attIt->second.name.c_str(), vitIt->second.name.c_str(),
           event.weaponId.c_str(), event.distanceMeters,
           (long long)event.attackerId, (long long)event.victimId);
}

void emitNpcKillPersistenceEvent(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t attackerPlayerId,
    uint32_t npcEntityId,
    uint32_t weaponId,
    uint32_t serverTick,
    const glm::vec3& attackerPos,
    const glm::vec3& npcPos)
{
    emitNpcKillPersistenceEvent(players, attackerPlayerId, npcEntityId,
        std::string(weaponIdToName(weaponId)), serverTick, attackerPos, npcPos);
}

void emitNpcKillPersistenceEvent(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t attackerPlayerId,
    uint32_t npcEntityId,
    const std::string& weaponName,
    uint32_t serverTick,
    const glm::vec3& attackerPos,
    const glm::vec3& npcPos)
{
    auto attIt = players.find(attackerPlayerId);
    if (attIt == players.end())
        return;

    PersistenceKillEvent event;
    event.eventId = makeEventId("npc_kill", serverTick, attackerPlayerId, npcEntityId);
    event.serverTick = serverTick;
    event.attackerType = "player";
    event.attackerId = resolveAccountId(attIt->second);
    event.attackerName = attIt->second.name;
    event.victimType = "npc";
    event.victimId = 0;
    event.victimName = "NPC";
    event.weaponId = weaponName;
    event.distanceMeters = distanceBetween(attackerPos, npcPos);

    PersistenceQueue::instance().enqueueKill(event);
}

} // namespace MimitaNet
