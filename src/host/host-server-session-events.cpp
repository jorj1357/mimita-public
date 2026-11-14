// 2026-11-13
// Resident reliable-event id/session owner and enqueue adapter.

#include "host/host-server-session-events.h"

#include <cstring>
#include <utility>
#include <vector>

#include "host/host-server-transport-adapter.h"
#include "network/net_common.h"
#include "network/packets.h"

namespace mimita::host {
namespace {

constexpr std::size_t kEventIdOffset = sizeof(MimitaNet::PacketHeader);
constexpr std::size_t kEventSessionOffset = kEventIdOffset + sizeof(std::uint32_t);
constexpr std::size_t kMinimumEventBytes = kEventSessionOffset + sizeof(std::uint32_t);

void write_u32(std::vector<char>& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

bool send_to_player(std::uint32_t playerId, const void* data, std::size_t size) {
    auto& transport = HostServerTransportAdapter::instance();
    const TransportAdapterV1* adapter = transport.adapter();
    if (!adapter || !adapter->send_to_player) {
        return false;
    }
    return adapter->send_to_player(transport.owner(), playerId, data, size);
}

}  // namespace

HostServerSessionEvents& HostServerSessionEvents::instance() {
    static HostServerSessionEvents events;
    return events;
}

HostServerSessionEvents::HostServerSessionEvents() {
    capability_.version = kSessionEventsCapabilityVersion;
    capability_.structSize =
        static_cast<std::uint32_t>(sizeof(SessionEventsCapabilityV1));
    capability_.next_event_id = &HostServerSessionEvents::nextEventIdThunk;
    capability_.session_id = &HostServerSessionEvents::sessionIdThunk;
    capability_.queue_to_player = &HostServerSessionEvents::queueToPlayerThunk;
    capability_.queue_to_all = &HostServerSessionEvents::queueToAllThunk;
    capability_.reset_player = &HostServerSessionEvents::resetPlayerThunk;
}

void HostServerSessionEvents::bindPlayers(
    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players) {
    players_ = players;
}

void HostServerSessionEvents::clear() {
    players_ = nullptr;
}

std::uint32_t HostServerSessionEvents::nextEventIdThunk(void* owner) {
    auto* events = static_cast<HostServerSessionEvents*>(owner);
    if (!events) {
        return 0;
    }
    std::uint32_t id = events->nextEventId_++;
    if (events->nextEventId_ == 0) {
        events->nextEventId_ = 1;
    }
    return id;
}

std::uint32_t HostServerSessionEvents::sessionIdThunk(void* owner,
                                                      std::uint32_t playerId) {
    auto* events = static_cast<HostServerSessionEvents*>(owner);
    if (!events || !events->players_) {
        return 0;
    }
    auto it = events->players_->find(playerId);
    if (it == events->players_->end()) {
        return 0;
    }
    auto& player = it->second;
    if (player.reliableEventSessionId == 0) {
        player.reliableEventSessionId = events->nextSessionId_++;
        if (player.reliableEventSessionId == 0) {
            player.reliableEventSessionId = events->nextSessionId_++;
        }
    }
    return player.reliableEventSessionId;
}

SessionEventQueueResult HostServerSessionEvents::queueToPlayer(
    std::uint32_t playerId, const std::uint8_t* bytes, std::uint32_t size,
    std::uint32_t eventId, std::uint32_t maxPending) {
    if (!players_ || !bytes || size < kMinimumEventBytes || eventId == 0 ||
        maxPending == 0) {
        return SessionEventQueueResult::InvalidArgument;
    }
    auto it = players_->find(playerId);
    if (it == players_->end()) {
        return SessionEventQueueResult::Unavailable;
    }
    auto& player = it->second;

    for (auto& pending : player.pendingReliableEvents) {
        if (pending.eventId != eventId) {
            continue;
        }
        pending.lastSendMs = MimitaNet::nowMs();
        ++pending.attempts;
        return send_to_player(playerId, pending.bytes.data(),
                              pending.bytes.size())
                   ? SessionEventQueueResult::Ok
                   : SessionEventQueueResult::Unavailable;
    }

    if (player.pendingReliableEvents.size() >= maxPending) {
        return SessionEventQueueResult::Saturated;
    }
    const std::uint32_t session = sessionIdThunk(this, playerId);
    if (session == 0) {
        return SessionEventQueueResult::InvalidArgument;
    }

    std::vector<char> packetBytes(
        reinterpret_cast<const char*>(bytes),
        reinterpret_cast<const char*>(bytes) + size);
    write_u32(packetBytes, kEventIdOffset, eventId);
    write_u32(packetBytes, kEventSessionOffset, session);

    const auto* header =
        reinterpret_cast<const MimitaNet::PacketHeader*>(bytes);
    MimitaNet::ServerPlayer::PendingReliableEvent pending;
    pending.eventId = eventId;
    pending.eventSessionId = session;
    pending.packetType = header->type;
    pending.createdMs = MimitaNet::nowMs();
    pending.lastSendMs = pending.createdMs;
    pending.attempts = 1;
    pending.bytes = std::move(packetBytes);
    player.pendingReliableEvents.push_back(std::move(pending));

    if (!send_to_player(playerId,
                        player.pendingReliableEvents.back().bytes.data(),
                        player.pendingReliableEvents.back().bytes.size())) {
        player.pendingReliableEvents.pop_back();
        return SessionEventQueueResult::Unavailable;
    }
    return SessionEventQueueResult::Ok;
}

SessionEventQueueResult HostServerSessionEvents::queueToAllThunk(
    void* owner, const std::uint8_t* bytes, std::uint32_t size,
    std::uint32_t eventId, std::uint32_t maxPending) {
    auto* events = static_cast<HostServerSessionEvents*>(owner);
    if (!events) {
        return SessionEventQueueResult::InvalidArgument;
    }
    if (!events->players_) {
        return SessionEventQueueResult::InvalidArgument;
    }
    SessionEventQueueResult result = SessionEventQueueResult::Ok;
    bool any = false;
    for (const auto& entry : *events->players_) {
        any = true;
        const SessionEventQueueResult one =
            events->queueToPlayer(entry.first, bytes, size, eventId,
                                  maxPending);
        if (one == SessionEventQueueResult::Saturated) {
            result = SessionEventQueueResult::Saturated;
        } else if (one == SessionEventQueueResult::Unavailable &&
                   result == SessionEventQueueResult::Ok) {
            result = SessionEventQueueResult::Unavailable;
        }
    }
    return any ? result : SessionEventQueueResult::Ok;
}

HostResult HostServerSessionEvents::resetPlayerThunk(
    void* owner, std::uint32_t playerId) {
    auto* events = static_cast<HostServerSessionEvents*>(owner);
    if (!events || !events->players_) {
        return HostResult::InvalidArgument;
    }
    auto it = events->players_->find(playerId);
    if (it == events->players_->end()) {
        return HostResult::NotFound;
    }
    it->second.pendingReliableEvents.clear();
    it->second.reliableEventSessionId = 0;
    return HostResult::Ok;
}

SessionEventQueueResult HostServerSessionEvents::queueToPlayerThunk(
    void* owner, std::uint32_t playerId, const std::uint8_t* bytes,
    std::uint32_t size, std::uint32_t eventId, std::uint32_t maxPending) {
    auto* events = static_cast<HostServerSessionEvents*>(owner);
    if (!events) {
        return SessionEventQueueResult::InvalidArgument;
    }
    return events->queueToPlayer(playerId, bytes, size, eventId, maxPending);
}

}  // namespace mimita::host
