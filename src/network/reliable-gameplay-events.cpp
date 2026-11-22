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
#include "debug/structured-log.h"
#include "config/networking-config.h"
#if defined(MIMITA_GAME_DLL)
#include "game/session-events-bridge-module.h"
#else
#include "host/host-server-session-events.h"
#endif

#include <algorithm>
#include <cstring>
#include <nlohmann/json.hpp>

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

// A reliable gameplay event that cannot be delivered is NOT proof that the
// peer is gone. The normal connection-health owner (timeout/grace) owns slot
// removal; erasing here made a healthy session vanish inside its reconnect
// grace as soon as the queue hit max attempts. Record the degraded delivery,
// drop the undeliverable event, and keep the slot/token for reconnect.
void noteReliableDeliveryFailure(const ServerPlayer& player,
                                 const char* trigger,
                                 uint8_t packetType,
                                 uint32_t eventId,
                                 size_t playersBefore)
{
    const uint64_t removalTimeMs = reliableNowMs();
    const uint32_t lastAcceptedServerTick = player.lastAcceptedServerTick;
    const char* failureTrigger = trigger ? trigger : "unspecified";
    const uint64_t silentMs = removalTimeMs >= player.lastHeardMs
        ? removalTimeMs - player.lastHeardMs : 0;

    // A dead peer can expire hundreds of queued events during its reconnect
    // grace. Flushing one Important record per event turns the grace window
    // into a journal-lock stall, so keep the structured diagnostic to one line
    // per second; the throttled aggregate below still counts every failure.
    static thread_local uint64_t lastStructuredLogMs = 0;
    if (removalTimeMs >= lastStructuredLogMs + 1000)
    {
        lastStructuredLogMs = removalTimeMs;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "network.server.reliable-event.delivery-degraded",
            "player:" + std::to_string(player.id) + ":event:" + std::to_string(eventId),
            failureTrigger, lastAcceptedServerTick,
            nlohmann::json{{"player_id", player.id},
                           {"failure_trigger", failureTrigger},
                           {"packet_type", (unsigned)packetType},
                           {"event_id", eventId},
                           {"event_session_id", player.reliableEventSessionId},
                           {"pending_reliable_events", player.pendingReliableEvents.size()},
                           {"last_accepted_server_tick", lastAcceptedServerTick},
                           {"tick_basis", "last_accepted_server_tick"},
                           {"last_heard_ms", player.lastHeardMs},
                           {"silent_ms", silentMs},
                           {"connection_stale", player.connectionStale},
                           {"slot_state", "held"},
                           {"action", "keep-connection"},
                           {"players_before", playersBefore},
                           {"players_after", playersBefore}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    logReliableFailureAggregate(trigger, player, packetType, eventId);
}

} // namespace

uint32_t serverReliableEventSessionId()
{
    static const uint32_t sessionId = makeSessionId();
    return sessionId;
}

uint32_t nextReliableGameplayEventId()
{
#if defined(MIMITA_GAME_DLL)
    const uint32_t hostId = mimita::game::nextSessionEventId();
    if (hostId != 0)
        return hostId;
#else
    auto& events = mimita::host::HostServerSessionEvents::instance();
    const uint32_t hostId =
        events.capability()->next_event_id(events.owner());
    if (hostId != 0)
        return hostId;
#endif
    static uint32_t nextId = 1;
    uint32_t id = nextId++;
    if (nextId == 0)
        nextId = 1;
    return id;
}

uint32_t reliableGameplayEventSessionForPlayer(ServerPlayer& player)
{
#if defined(MIMITA_GAME_DLL)
    const uint32_t hostSession =
        mimita::game::sessionEventSessionId(player.id);
    if (hostSession != 0) {
        player.reliableEventSessionId = hostSession;
        return hostSession;
    }
#endif
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

    // Chat request retries reuse the server's cached event. Do not append a
    // second queue entry for the same event while the first copy is still
    // awaiting its ACK; that would multiply retries and consume the backlog.
    if (eventId != 0)
    {
        for (auto& pending : player.pendingReliableEvents)
        {
            if (pending.eventId != eventId)
                continue;

            const uint64_t now = reliableNowMs();
            pending.lastSendMs = now;
            ++pending.attempts;
            const bool sent = serverSendToPlayer(sock, player,
                                                 pending.bytes.data(),
                                                 pending.bytes.size());
            if (!sent)
            {
                ++gFailureStats.sendFailed;
                return ReliableGameplayEventQueueResult::ConnectionUnavailable;
            }
            ++totalPacketsOut;
            return ReliableGameplayEventQueueResult::Queued;
        }
    }

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
            noteReliableDeliveryFailure(it->second,
                one == ReliableGameplayEventQueueResult::BacklogSaturated
                    ? "backlog-saturated" : "initial-send-failed",
                header->type, eventId, players.size());
            result = worseResult(result, one);
            ++it;
            continue;
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

    for (auto playerIt = players.begin(); playerIt != players.end(); ++playerIt)
    {
        ServerPlayer& player = playerIt->second;
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

                // Keep the connection; the normal server connection-health
                // timer owns disconnects. Dropping only the undeliverable
                // event prevents an unbounded retry backlog without erasing a
                // session that is still inside its reconnect grace window.
                noteReliableDeliveryFailure(player,
                    ttlExpired ? "ttl-expired" : "attempts-exhausted",
                    it->packetType, it->eventId, players.size());
                it = player.pendingReliableEvents.erase(it);
                continue;
            }
            if (now - it->lastSendMs >= (uint64_t)cfg.retryMs)
            {
                const bool sent = serverSendToPlayer(sock, player, it->bytes.data(), it->bytes.size());
                if (!sent)
                {
                    ++gFailureStats.sendFailed;
                    ++expiredCount;
                    // Transport is temporarily unavailable. Keep the slot and
                    // pending event; retry next maintenance tick or let TTL
                    // bound it. Stop retrying this player for this tick.
                    noteReliableDeliveryFailure(player, "retry-send-failed",
                                                it->packetType, it->eventId,
                                                players.size());
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
