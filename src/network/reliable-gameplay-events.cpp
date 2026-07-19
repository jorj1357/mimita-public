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

#include <algorithm>

namespace MimitaNet {
namespace {

constexpr size_t RELIABLE_EVENT_MAX_PENDING_PER_PLAYER = 64;
constexpr uint64_t RELIABLE_EVENT_RETRY_MS = 100;
constexpr uint64_t RELIABLE_EVENT_TTL_MS = 10000;
constexpr uint8_t RELIABLE_EVENT_MAX_ATTEMPTS = 80;

uint32_t makeSessionId()
{
    uint64_t t = nowMs();
    uint32_t v = (uint32_t)(t ^ (t >> 32) ^ 0x4d494d38u);
    return v ? v : 1;
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

void queueReliableGameplayEventToAll(SOCKET sock,
                                     std::unordered_map<uint32_t, ServerPlayer>& players,
                                     const void* data,
                                     size_t size,
                                     uint32_t eventId,
                                     uint32_t eventSessionId,
                                     uint64_t& totalPacketsOut)
{
    if (!data || size < sizeof(PacketHeader) || eventId == 0 || eventSessionId == 0)
        return;

    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
    const uint64_t now = nowMs();
    for (auto& entry : players)
    {
        ServerPlayer& player = entry.second;
        if (player.pendingReliableEvents.size() >= RELIABLE_EVENT_MAX_PENDING_PER_PLAYER)
            player.pendingReliableEvents.pop_front();

        ServerPlayer::PendingReliableEvent pending;
        pending.eventId = eventId;
        pending.eventSessionId = eventSessionId;
        pending.packetType = header->type;
        pending.createdMs = now;
        pending.lastSendMs = now;
        pending.attempts = 1;
        pending.bytes.assign((const char*)data, (const char*)data + size);
        player.pendingReliableEvents.push_back(std::move(pending));

        serverSendToPlayer(sock, player, data, size);
        ++totalPacketsOut;
    }
}

void handleReliableEventAck(const char* buffer, int bytes,
                            std::unordered_map<uint32_t, ServerPlayer>& players)
{
    if (bytes < (int)sizeof(ReliableEventAckPacket))
        return;
    const ReliableEventAckPacket* ack = reinterpret_cast<const ReliableEventAckPacket*>(buffer);
    auto playerIt = players.find(ack->header.playerId);
    if (playerIt == players.end())
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
    const uint64_t now = nowMs();
    uint32_t pendingCount = 0;
    uint32_t resentCount = 0;
    uint32_t expiredCount = 0;

    for (auto& entry : players)
    {
        ServerPlayer& player = entry.second;
        for (auto it = player.pendingReliableEvents.begin(); it != player.pendingReliableEvents.end(); )
        {
            if (now - it->createdMs > RELIABLE_EVENT_TTL_MS || it->attempts >= RELIABLE_EVENT_MAX_ATTEMPTS)
            {
                it = player.pendingReliableEvents.erase(it);
                ++expiredCount;
                continue;
            }
            if (now - it->lastSendMs >= RELIABLE_EVENT_RETRY_MS)
            {
                serverSendToPlayer(sock, player, it->bytes.data(), it->bytes.size());
                it->lastSendMs = now;
                ++it->attempts;
                ++totalPacketsOut;
                ++resentCount;
            }
            ++pendingCount;
            ++it;
        }
    }

    static uint64_t lastLogMs = 0;
    if ((pendingCount || resentCount || expiredCount) && now - lastLogMs >= 1000)
    {
        Debug::logThrottled(Debug::Category::Networking, "reliable-gameplay-events", 1.0f,
                            "[RELIABLE EVENT] pending=%u resent=%u expired=%u retryMs=%llu ttlMs=%llu\n",
                            pendingCount, resentCount, expiredCount,
                            (unsigned long long)RELIABLE_EVENT_RETRY_MS,
                            (unsigned long long)RELIABLE_EVENT_TTL_MS);
        lastLogMs = now;
    }
}

} // namespace MimitaNet
