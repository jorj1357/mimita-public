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
#include "debug/debug-log.h"

#include <cstdio>
#include <sstream>
#include <iomanip>

namespace MimitaNet {

void tickServerProgression(SOCKET sock, std::unordered_map<uint32_t, ServerPlayer>& players,
                           bool worldActive, uint64_t& totalPacketsOut)
{
    auto& queue = PersistenceQueue::instance();
    for (const auto& item : players)
    {
        const auto& p = item.second;
        queue.observePlayer(p.id, p.accountId, p.name, p.progressionTicket,
            worldActive && !p.connectionStale && p.spawnState != ServerPlayer::AwaitingMapReady);
    }
    queue.tick();
    for (const auto& notice : queue.takeNotices())
    {
        auto player = players.find(notice.playerId);
        if (player == players.end() || player->second.connectionStale) continue;
        ProgressionEventPacket packet{};
        packet.header.type = PACKET_PROGRESSION_EVENT;
        packet.kind = notice.kind;
        std::snprintf(packet.name, sizeof(packet.name), "%s", notice.name.c_str());
        std::snprintf(packet.confirmedAt, sizeof(packet.confirmedAt), "%s", notice.confirmedAt.c_str());
        queueReliableGameplayEventToPlayer(sock, player->second, &packet, sizeof(packet),
            nextReliableGameplayEventId(), reliableGameplayEventSessionForPlayer(player->second), totalPacketsOut);
    }
}

static std::string makeEventId(const char* prefix, uint32_t tick,
                               uint64_t attacker, uint64_t victim)
{
    std::ostringstream ss;
    ss << PersistenceQueue::instance().sessionId() << "_" << prefix << "_" << tick << "_" << attacker << "_" << victim;
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
    if (vitIt == players.end())
        return;

    PersistenceKillEvent event;
    // One death per victim life, independent of how many weapon callers report it.
    event.eventId = makeEventId("death", vitIt->second.spawnGeneration, 0, victimPlayerId);
    event.serverTick = serverTick;
    event.attackerPlayerId = attackerPlayerId;
    event.victimPlayerId = victimPlayerId;
    event.victimIdentity = victimPlayerId;
    event.deathGeneration = vitIt->second.spawnGeneration;
    event.attackerType = attIt == players.end() ? "environment" : "player";
    event.attackerId = attIt == players.end() ? 0 : resolveAccountId(attIt->second);
    event.attackerName = attIt == players.end() ? "environment" : attIt->second.name;
    event.victimType = "player";
    event.victimId = resolveAccountId(vitIt->second);
    event.victimName = vitIt->second.name;
    event.weaponId = weaponName;
    event.distanceMeters = distanceBetween(attackerPos, victimPos);

    PersistenceQueue::instance().enqueueKill(event);

    Debug::logThrottled(Debug::Category::Networking, "progression-death", 1.0f,
        "[PERSISTENCE] death event=%s attacker=%lld victim=%lld source=%s\n",
        event.eventId.c_str(), (long long)event.attackerId, (long long)event.victimId,
        event.attackerType.c_str());
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
    event.victimIdentity = (uint64_t(1) << 32) | npcEntityId;
    event.deathGeneration = serverTick;
    event.serverTick = serverTick;
    event.attackerPlayerId = attackerPlayerId;
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
