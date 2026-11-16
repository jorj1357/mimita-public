// 2026-11-16
#include "network/packet-route.h"

#include <array>
#include <atomic>
#include <string>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"

namespace MimitaNet {
namespace PacketRoute {
namespace {
std::array<std::atomic<bool>, 256> gClientSeen{};
std::array<std::atomic<bool>, 256> gServerSeen{};

const char* directionName(Direction direction) {
    return direction == Direction::Client ? "client" : "server";
}
}  // namespace

void record(Direction direction, std::uint8_t packetId, const char* path,
            const char* source) {
    auto& seen = direction == Direction::Client ? gClientSeen : gServerSeen;
    if (direction == Direction::Client && packetId < seen.size()) {
        if (seen[packetId].exchange(true, std::memory_order_acq_rel)) return;
    } else if (packetId < seen.size()) {
        if (seen[packetId].exchange(true, std::memory_order_acq_rel)) return;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "packet.route", std::to_string(packetId), "inbound packet route observed",
        0,
        nlohmann::json{{"packet_id", packetId},
                       {"direction", directionName(direction)},
                       {"path", path ? path : "unknown"},
                       {"source", source ? source : ""}},
        source ? source : __FILE__, 0, __func__);
}
}  // namespace PacketRoute
}  // namespace MimitaNet
