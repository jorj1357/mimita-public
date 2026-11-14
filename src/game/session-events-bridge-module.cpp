// 2026-11-13
// Game-DLL reliable-event identity/enqueue bridge.

#include "game/session-events-bridge-module.h"

namespace mimita::game {
namespace {

const mimita::host::SessionEventsCapabilityV1* gEvents = nullptr;
void* gHostData = nullptr;

}  // namespace

void initializeHostSessionEvents(const mimita::host::HostServicesV1* services) {
    gEvents = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability =
        static_cast<const mimita::host::SessionEventsCapabilityV1*>(
            services->get_capability(
                services->hostUserData,
                static_cast<std::uint32_t>(
                    mimita::host::HostCapabilityId::SessionEvents),
                mimita::host::kSessionEventsCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kSessionEventsCapabilityVersion &&
        capability->structSize >=
            sizeof(mimita::host::SessionEventsCapabilityV1)) {
        gEvents = capability;
    }
}

std::uint32_t nextSessionEventId() {
    if (!gEvents || !gEvents->next_event_id) {
        return 0;
    }
    return gEvents->next_event_id(gHostData);
}

std::uint32_t sessionEventSessionId(std::uint32_t playerId) {
    if (!gEvents || !gEvents->session_id) {
        return 0;
    }
    return gEvents->session_id(gHostData, playerId);
}

mimita::host::SessionEventQueueResult queueSessionEventToPlayer(
    std::uint32_t playerId, const void* bytes, std::uint32_t size,
    std::uint32_t eventId, std::uint32_t maxPending) {
    if (!gEvents || !gEvents->queue_to_player || !bytes) {
        return mimita::host::SessionEventQueueResult::InvalidArgument;
    }
    return gEvents->queue_to_player(
        gHostData, playerId,
        static_cast<const std::uint8_t*>(bytes), size, eventId, maxPending);
}

mimita::host::SessionEventQueueResult queueSessionEventToAll(
    const void* bytes, std::uint32_t size, std::uint32_t eventId,
    std::uint32_t maxPending) {
    if (!gEvents || !gEvents->queue_to_all || !bytes) {
        return mimita::host::SessionEventQueueResult::InvalidArgument;
    }
    return gEvents->queue_to_all(
        gHostData, static_cast<const std::uint8_t*>(bytes), size, eventId,
        maxPending);
}

mimita::host::HostResult resetSessionEventPlayer(std::uint32_t playerId) {
    if (!gEvents || !gEvents->reset_player) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEvents->reset_player(gHostData, playerId);
}

}  // namespace mimita::game
