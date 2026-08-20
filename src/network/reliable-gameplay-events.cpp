// 07 19 2026, 12 55
/* purpose
* Provide generic reliable-unordered delivery for critical gameplay events.
* Tracks per-player unacknowledged event packets and retransmits at a bounded rate.
* Uses eventId plus server event session ID for safe deduplication and ACKs.
* Does NOT define projectile behavior, damage rules, or visual presentation.
* Does NOT provide ordered streams, congestion control, or snapshot reliability.
* Does NOT trust client-reported gameplay outcomes.
*/

#include "network/server.h"
#include "debug/debug-log.h"
#include "config/networking-config.h"

#include <algorithm>
#include <cstring>

namespace MimitaNet {
namespace {

constexpr size_t RELIABLE_EVENT_ID_OFFSET = sizeof(PacketHeader);
constexpr size_t RELIABLE_EVENT_SESSION_OFFSET = sizeof(PacketHeader) + sizeof(uint32_t);

uint64_t gTestNowMs = 0;

struct ReliableFailureStats
{
    uint32_t saturated = 0;
    uint32_t sendFailed = 0;
    uint32_t attemptsExhausted = 0;
    uint32_t ttlExpired = 0;
};

ReliableFailureStats gFailureStats;

uint64_t reliableNowMs()
{
    return gTestNowMs ? gTestNowMs : nowMs();
}

uint32_t makeSessionId()
{
    uint64_t t = nowMs();
    uint32_t v = (uint32_t)(t ^ (t >> 32) ^ 0x4d494d38u);
    return v ? v : 1;
}

uint32_t nextSessionId()
{
    static uint32_t nextId = makeSessionId();
    uint32_t id = nextId++;
    if (nextId == 0)
        nextId = 1;
    return id ? id : 1;
}

uint32_t& mutableEventId(char* data)
{
    return *reinterpret_cast<uint32_t*>(data + RELIABLE_EVENT_ID_OFFSET);
}

uint32_t& mutableEventSessionId(char* data)
{
    return *reinterpret_cast<uint32_t*>(data + RELIABLE_EVENT_SESSION_OFFSET);
}

bool sameReliableAckAddress(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_family == b.sin_family &&
        a.sin_addr.s_addr == b.sin_addr.s_addr &&
        a.sin_port == b.sin_port;
}

ReliableGameplayEventQueueResult worseResult(ReliableGameplayEventQueueResult a,
                                             ReliableGameplayEventQueueResult b)
{
    if (a == ReliableGameplayEventQueueResult::ConnectionUnavailable ||
        b == ReliableGameplayEventQueueResult::ConnectionUnavailable)
        return ReliableGameplayEventQueueResult::ConnectionUnavailable;
    if (a == ReliableGameplayEventQueueResult::BacklogSaturated ||
        b == ReliableGameplayEventQueueResult::BacklogSaturated)
        return ReliableGameplayEventQueueResult::BacklogSaturated;
    return ReliableGameplayEventQueueResult::Queued;
}

void logReliableFailureAggregate(const char* trigger,
                                 const ServerPlayer& player,
                                 uint8_t packetType,
                                 uint32_t eventId)
{
    Debug::logThrottled(Debug::Category::Networking, "reliable-event-fail-aggregate", 1.0f,
                        "[RELIABLE EVENT FAIL] trigger=%s playerId=%u packetType=%u eventId=%u action=disconnect saturated=%u sendFailed=%u attemptsExhausted=%u ttlExpired=%u\n",
                        trigger, player.id, (unsigned)packetType, eventId,
                        gFailureStats.saturated, gFailureStats.sendFailed,
                        gFailureStats.attemptsExhausted, gFailureStats.ttlExpired);
}

void markReliableConnectionUnhealthy(std::unordered_map<uint32_t, ServerPlayer>& players,
                                     std::unordered_map<uint32_t, ServerPlayer>::iterator& it,
                                     const char* trigger,
                                     uint8_t packetType,
                                     uint32_t eventId)
{
    logReliableFailureAggregate(trigger, it->second, packetType, eventId);
    it = players.erase(it);
}

} // namespace

uint32_t serverReliableEventSessionId()
{
    static const uint32_t sessionId = makeSessionId();
    return sessionId;
}

uint32_t nextReliableGameplayEventId()
{
    static uint32_t nextId = 1;
    uint32_t id = nextId++;
    if (nextId == 0)
        nextId = 1;
    return id;
}

uint32_t reliableGameplayEventSessionForPlayer(ServerPlayer& player)
{
    if (player.reliableEventSessionId == 0)
        player.reliableEventSessionId = nextSessionId();
    return player.reliableEventSessionId;
}

namespace {

// Enqueue one reliable event for a single player and send it immediately.
// Does NOT disconnect on failure; the broadcast path handles disconnects in
// its own loop (it must erase from the player map safely).
ReliableGameplayEventQueueResult queueForOnePlayer(
    SOCKET sock,
    ServerPlayer& player,
    const void* data,
    size_t size,
    uint32_t eventId,
    uint64_t& totalPacketsOut)
{
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
    const auto& cfg = NetworkingConfig::instance().data().reliableEvents;
    if (player.pendingReliableEvents.size() >= cfg.maxPendingPerPlayer)
    {
        ++gFailureStats.saturated;
        Debug::logThrottled(Debug::Category::Networking, "reliable-gameplay-events-full", 1.0f,
                            "[RELIABLE EVENT SATURATED] playerId=%u pending=%zu max=%zu packetType=%u eventId=%u\n",
                            player.id, player.pendingReliableEvents.size(),
                            cfg.maxPendingPerPlayer,
                            (unsigned)header->type, eventId);
        return ReliableGameplayEventQueueResult::BacklogSaturated;
    }

    reliableGameplayEventSessionForPlayer(player);

    std::vector<char> packetBytes((const char*)data, (const char*)data + size);
    mutableEventId(packetBytes.data()) = eventId;
    mutableEventSessionId(packetBytes.data()) = player.reliableEventSessionId;

    ServerPlayer::PendingReliableEvent pending;
    pending.eventId = eventId;
    pending.eventSessionId = player.reliableEventSessionId;
    pending.packetType = header->type;
    pending.createdMs = reliableNowMs();
    pending.lastSendMs = reliableNowMs();
    pending.attempts = 1;
    pending.bytes = std::move(packetBytes);
    player.pendingReliableEvents.push_back(std::move(pending));
    Debug::log(Debug::Category::Networking,
        "[DuelPacketSend] type=%u reliable=1 player=%u event=%u session=%u attempt=initial\n",
        (unsigned)header->type, player.id, eventId, player.reliableEventSessionId);

    const bool sent = serverSendToPlayer(sock, player,
                                        player.pendingReliableEvents.back().bytes.data(),
                                        player.pendingReliableEvents.back().bytes.size());
    if (!sent)
    {
        ++gFailureStats.sendFailed;
        player.pendingReliableEvents.pop_back();
        return ReliableGameplayEventQueueResult::ConnectionUnavailable;
    }
    ++totalPacketsOut;
    return ReliableGameplayEventQueueResult::Queued;
}

} // namespace

ReliableGameplayEventQueueResult queueReliableGameplayEventToAll(SOCKET sock,
                                                                 std::unordered_map<uint32_t, ServerPlayer>& players,
                                                                 const void* data,
                                                                 size_t size,
                                                                 uint32_t eventId,
                                                                 uint32_t eventSessionId,
                                                                 uint64_t& totalPacketsOut)
{
    if (!data || size < RELIABLE_EVENT_SESSION_OFFSET + sizeof(uint32_t) || eventId == 0)
        return ReliableGameplayEventQueueResult::ConnectionUnavailable;

    ReliableGameplayEventQueueResult result = ReliableGameplayEventQueueResult::Queued;
    for (auto it = players.begin(); it != players.end(); )
    {
        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
        ReliableGameplayEventQueueResult one =
            queueForOnePlayer(sock, it->second, data, size, eventId,
                              totalPacketsOut);
        if (one == ReliableGameplayEventQueueResult::ConnectionUnavailable ||
            one == ReliableGameplayEventQueueResult::BacklogSaturated)
        {
            markReliableConnectionUnhealthy(players, it,
                one == ReliableGameplayEventQueueResult::BacklogSaturated
                    ? "backlog-saturated" : "initial-send-failed",
                header->type, eventId);
            result = worseResult(result, one);
            continue;  // markReliableConnectionUnhealthy already advanced `it`
        }
        result = worseResult(result, one);
        ++it;
    }

    (void)eventSessionId;
    return result;
}

ReliableGameplayEventQueueResult queueReliableGameplayEventToPlayer(
    SOCKET sock,
    ServerPlayer& player,
    const void* data,
    size_t size,
    uint32_t eventId,
    uint32_t eventSessionId,
    uint64_t& totalPacketsOut)
{
    if (!data || size < RELIABLE_EVENT_SESSION_OFFSET + sizeof(uint32_t) || eventId == 0)
        return ReliableGameplayEventQueueResult::ConnectionUnavailable;

    (void)eventSessionId;
    return queueForOnePlayer(sock, player, data, size, eventId,
                             totalPacketsOut);
}

void handleReliableEventAck(const char* buffer, int bytes,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            const sockaddr_in* from,
                            const ServerPlayer* authenticatedPlayer)
{
    if (bytes < (int)sizeof(ReliableEventAckPacket))
        return;
    const ReliableEventAckPacket* ack = reinterpret_cast<const ReliableEventAckPacket*>(buffer);
    auto playerIt = players.find(ack->header.playerId);
    if (playerIt == players.end())
        return;
    if (authenticatedPlayer && authenticatedPlayer->id != ack->header.playerId)
        return;
    if (from && !sameReliableAckAddress(playerIt->second.addr, *from))
        return;
    if (ack->eventSessionId == 0 || ack->eventSessionId != playerIt->second.reliableEventSessionId)
        return;
    auto& pending = playerIt->second.pendingReliableEvents;
    pending.erase(std::remove_if(pending.begin(), pending.end(),
        [&](const ServerPlayer::PendingReliableEvent& e) {
            return e.eventId == ack->eventId && e.eventSessionId == ack->eventSessionId;
        }), pending.end());
}

void tickReliableGameplayEvents(SOCKET sock,
                                std::unordered_map<uint32_t, ServerPlayer>& players,
                                uint64_t& totalPacketsOut)
{
    const uint64_t now = reliableNowMs();
    const auto& cfg = NetworkingConfig::instance().data().reliableEvents;
    uint32_t pendingCount = 0;
    uint32_t resentCount = 0;
    uint32_t expiredCount = 0;

    for (auto playerIt = players.begin(); playerIt != players.end(); )
    {
        ServerPlayer& player = playerIt->second;
        bool disconnected = false;
        for (auto it = player.pendingReliableEvents.begin(); it != player.pendingReliableEvents.end(); )
        {
            const bool ttlExpired = now - it->createdMs > (uint64_t)cfg.ttlMs;
            const bool attemptsExhausted = it->attempts >= cfg.maxAttempts;
            if (ttlExpired || attemptsExhausted)
            {
                if (ttlExpired)
                    ++gFailureStats.ttlExpired;
                if (attemptsExhausted)
                    ++gFailureStats.attemptsExhausted;
                ++expiredCount;
                markReliableConnectionUnhealthy(players, playerIt,
                    ttlExpired ? "ttl-expired" : "attempts-exhausted",
                    it->packetType, it->eventId);
                disconnected = true;
                break;
            }
            if (now - it->lastSendMs >= (uint64_t)cfg.retryMs)
            {
                const bool sent = serverSendToPlayer(sock, player, it->bytes.data(), it->bytes.size());
                if (!sent)
                {
                    ++gFailureStats.sendFailed;
                    ++expiredCount;
                    markReliableConnectionUnhealthy(players, playerIt,
                        "retry-send-failed", it->packetType, it->eventId);
                    disconnected = true;
                    break;
                }
                it->lastSendMs = now;
                ++it->attempts;
                ++totalPacketsOut;
                Debug::log(Debug::Category::Networking,
                    "[DuelPacketSend] type=%u reliable=1 player=%u event=%u session=%u attempt=retry\n",
                    (unsigned)it->packetType, player.id, it->eventId, it->eventSessionId);
                ++resentCount;
            }
            ++pendingCount;
            ++it;
        }
        if (!disconnected)
            ++playerIt;
    }

    static uint64_t lastLogMs = 0;
    if ((pendingCount || resentCount || expiredCount) && now - lastLogMs >= 1000)
    {
        Debug::logThrottled(Debug::Category::Networking, "reliable-gameplay-events", 1.0f,
                            "[RELIABLE EVENT] pending=%u resent=%u expired=%u retryMs=%llu ttlMs=%llu\n",
                            pendingCount, resentCount, expiredCount,
                            (unsigned long long)cfg.retryMs,
                            (unsigned long long)cfg.ttlMs);
        lastLogMs = now;
    }
}

void setReliableGameplayEventTestNowMs(uint64_t nowMsOverride)
{
    gTestNowMs = nowMsOverride;
}

} // namespace MimitaNet
